#include "IVideoStreamer.h"
#include <iostream>
#include <fstream>

/* Support older versions of ffmpeg and libav */
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(52, 64, 0)
  #define AVMEDIA_TYPE_AUDIO CODEC_TYPE_AUDIO
  #define AVMEDIA_TYPE_VIDEO CODEC_TYPE_VIDEO
  #define AV_PKT_FLAG_KEY PKT_FLAG_KEY
#endif

#undef AV_NOPTS_VALUE
#define AV_NOPTS_VALUE int64_t(0x8000000000000000)
#define VAR_SWAP(a,b,t) ((t) = (a), (a) = (b), (b) = (t))

VideoStreamer::VideoStreamer(int ai_bufferSize)
	: m_formatContext(NULL) 
	, m_vStream(NULL)
	, m_vOutBuf(NULL)
	, m_vOutBufSize(ai_bufferSize)
        , m_stoprequested(false)
        , m_thread(this)
        , m_movieCount(0)
{
	m_frame[0] = NULL;
	m_frame[1] = NULL;

	// Initialize libavcodec, and register all codecs and formats
	avcodec_init();
	avcodec_register_all();
	av_register_all();

	

	// Save sdp info to a file
        /*std::fstream sdpFile;
	sdpFile.open ("videostreamer.sdp", std::fstream::out | std::fstream::binary);
	char sdp[2048];
	avf_sdp_create(&m_formatContext, 1, sdp, 2048);
	sdpFile << sdp;
        sdpFile.close();*/

        checkAddEncoder(CODEC_ID_MPEG4,"mp4","MPEG4 in mp4");
        checkAddEncoder(CODEC_ID_H264,"mp4","H.264 in mp4");
        checkAddEncoder(CODEC_ID_MPEG4,"avi","MPEG4 in avi");
        checkAddEncoder(CODEC_ID_MPEG4,"mov","MPEG4 in mov");
        dvComboEntry=checkAddEncoder(CODEC_ID_DVVIDEO,"dv","DV in dv");
        checkAddEncoder(CODEC_ID_MSMPEG4V2,"avi","MPEG4 MS-V2 in avi");

        SetupVideo();


}

int VideoStreamer::checkAddEncoder(CodecID id,const char *ext,std::string displayname){
    if(avcodec_find_encoder(id)){
        encoderNames.push_back(displayname);
        codecs.push_back(std::pair<CodecID,const char *> (id,ext));
        return codecs.size()-1;
    }
    fprintf(stderr,"Cant find %d\n",id);
    return -1;
}

VideoStreamer::~VideoStreamer(void)
{
    if(_isOpen)
        CloseVideo();


}

void VideoStreamer::SetupVideo(std::string dir,
                               std::string baseName,
                               unsigned int codec_num,
                               unsigned int width,
                               unsigned int height,
                               int bitrate,
                               int frameRate,
                               int gopSize,	// emit one I-frame every "ai_gopSize" frames at most
                               int bFrames    //his number of B-Frames in each gop
                               )

{
    ai_dir=dir;
    ai_baseName=baseName;
ai_width=width;
ai_height=height;
ai_bitRate=bitrate;
ai_frameRate=frameRate;
ai_gopSize=gopSize;
ai_bFrames=bFrames;
ai_format=getFormat(codec_num);
ai_codec_id=getFFMPEGCodecId(codec_num);

}
void VideoStreamer::ReleaseContext(){
    // Release the streams
    if (m_formatContext)
    {
        for(unsigned int i = 0; i < m_formatContext->nb_streams; i++)
        {
            av_freep(&m_formatContext->streams[i]->codec);
            av_freep(&m_formatContext->streams[i]);
        }

        // Release the format context
        av_free(m_formatContext);
    }

}
// Open the output file
const char *VideoStreamer::getFormat(unsigned int codec_id){
   if(codec_id < codecs.size())
       return codecs[codec_id].second;
    return NULL;
}

CodecID VideoStreamer::getFFMPEGCodecId(unsigned int codec_id){
   if(codec_id < codecs.size())
       return codecs[codec_id].first;
    return (CodecID)NULL;
}

int VideoStreamer::OpenVideo(void)
{
    //Check if allready open
    if(_isOpen)
        return -1;
    printf("Opening Video\n");
    m_stoprequested=false;


    char ai_fileName[1024];
    sprintf(ai_fileName,"%s/%s-%02d.%s",ai_dir.c_str(),ai_baseName.c_str(),m_movieCount,ai_format);
    // Allocate and initialize format context.
    m_formatContext = CreateFormatContext(ai_fileName, ai_format, NULL, NULL,ai_codec_id);
    if (!m_formatContext)
        return -1;

    // Add video stream
    if (m_formatContext->oformat->video_codec != CODEC_ID_NONE)
        m_vStream = CreateVideoStream(
                m_formatContext,
                ai_width,
                ai_height,
                ai_bitRate,
                ai_frameRate,
                ai_gopSize,
                ai_bFrames,
                PIX_FMT_YUV420P);

    // Set the output parameters (must be done even if no parameters)
    av_set_parameters(m_formatContext, NULL );

    // Write out the format to the console
    dump_format(m_formatContext, 0, ai_fileName, 1);


    // Find the video encoder
    AVCodecContext *c = m_vStream->codec;
    AVCodec *codec = avcodec_find_encoder(c->codec_id);
    if (!codec)
    {
        std::cerr << "Error # VideoStreamer::OpenVideo: Codec not found" << std::endl;
        return -1;
    }

    // Open the codec
    if (avcodec_open(c, codec) < 0)
    {
        std::cerr << "Error # VideoStreamer::OpenVideo: Could not open codec" << std::endl;
        return -2;
    }

    // Allocate the encoded raw picture.
    if (c->pix_fmt != PIX_FMT_BGR24)
        m_frame[0] = CreateFrame(c->pix_fmt, c->width, c->height);
    else
        m_frame[0] = avcodec_alloc_frame();
    m_frame[1] = CreateFrame(c->pix_fmt, c->width, c->height);

    if (!m_frame[0] || !m_frame[1])
    {
		std::cerr << "Error # VideoStreamer::OpenVideo: Could not allocate picture" << std::endl;
		return -3;
	}

	// Allocate output buffer for encoded frames if needed
	if (!(m_formatContext->oformat->flags & AVFMT_RAWPICTURE))
		m_vOutBuf = (uint8_t*)av_malloc(m_vOutBufSize);
	if (!m_vOutBuf)
		std::cerr << "Error # VideoStreamer::OpenVideo: Can't allocate output buffer" << std::endl;

	// Open the output file, if needed
	if (!(m_formatContext->oformat->flags & AVFMT_NOFILE) && 
		 (url_fopen(&m_formatContext->pb, m_formatContext->filename, URL_WRONLY) < 0)) 
	{
		std::cerr << "Error # VideoStreamer::OpenVideo: Could not open '" << m_formatContext->filename << "'" << std::endl;
		return -4;
	}
        _isOpen=true;
        m_movieCount++;

        m_thread.start();
	return 0;
}

// Update the frame to stream
bool VideoStreamer::Update(ImgData* ai_image)
{
        if(!_isOpen)
            return false;
        OpenThreads::ScopedLock<OpenThreads::Mutex> lock_(m_updateMutex);
//	if (!ValidateImage(ai_image))
//		return false;

	// If codec pixel format is diferent from PIX_FMT_BGR24 convert image data
	AVCodecContext *c = m_vStream->codec;
	if (c->pix_fmt == PIX_FMT_BGR24)
	{
		avpicture_fill(
			(AVPicture *)m_frame, 
                        (uint8_t *)ai_image->data,
			PIX_FMT_BGR24, 
			ai_image->width, ai_image->height);
	}
	else
	{
		if (m_frame[0]->data == NULL)
			return false;

		// As we have a BGR24 picture, we must convert it to the codec pixel format if needed
		struct SwsContext *convContext = sws_getContext(
				ai_image->width, ai_image->height,
				PIX_FMT_BGR24,
				c->width, c->height,
				c->pix_fmt,
				SWS_BICUBIC, 
				NULL, NULL, NULL);

		if (convContext == NULL) 
		{
			std::cout << "Cannot initialize the conversion context" << std::endl;
			return false;
		}

                int linesize[4] = {ai_image->width*3,0,0,0};
		sws_scale(
			convContext,
                        (uint8_t **)&ai_image->data,
			linesize, 0, ai_image->height,
			m_frame[0]->data, m_frame[0]->linesize);

		sws_freeContext(convContext);
	}

        OpenThreads::ScopedLock<OpenThreads::Mutex> lock(m_mutex);
	AVFrame *tmp;
        VAR_SWAP(m_frame[0], m_frame[1], tmp);

	//m_frame[1] is now updated and will be used by the thread working method VideoStreamer::Run()
        m_firstFrame.signal();
	return true;
}


int VideoStreamer::Write(AVFrame *ai_picture)
{
	AVPacket packet;
	av_init_packet(&packet);
	packet.stream_index = m_vStream->index;

	if (m_formatContext->oformat->flags & AVFMT_RAWPICTURE) 
	{
		// Raw video case. The API will change slightly in the near	future for that
		packet.size = sizeof(AVPicture);
		packet.data = (uint8_t *)ai_picture;
        packet.flags |= AV_PKT_FLAG_KEY;
	} 
	else 
	{
		packet.size = avcodec_encode_video(m_vStream->codec, m_vOutBuf, m_vOutBufSize, ai_picture);
		if (packet.size > 0) // if zero size, it means the image was buffered
		{
			packet.data = m_vOutBuf;

			AVFrame *codedFrame = m_vStream->codec->coded_frame;

			// pts: Presentation time stamp in time_base units.
			if (codedFrame->pts != AV_NOPTS_VALUE)
				packet.pts = av_rescale_q(codedFrame->pts, m_vStream->codec->time_base, m_vStream->time_base);

			if (codedFrame->key_frame)
                packet.flags |= AV_PKT_FLAG_KEY;
		}
		else return 0;
	}

	return av_interleaved_write_frame(m_formatContext, &packet);
}



// Thread working method that writes out the AVFrames
void VideoStreamer::Run(void)
{
       // OpenThreads::ScopedLock<OpenThreads::Mutex> lock(m_mutex);
	AVRational &tb = m_vStream->codec->time_base;
	double cycle = 1e3 * tb.num / (double)tb.den;

	// Wait for the first frame update and send the format context
        m_firstFrame.wait(&m_mutex,100000000);
	av_write_header(m_formatContext);
        m_mutex.unlock();

        osg::Timer_t _startTick =  osg::Timer::instance()->tick();
	uint64_t framesSent = 0;

	while(!m_stoprequested)
	{
                uint64_t elapsedTime = osg::Timer::instance()->delta_m(_startTick, osg::Timer::instance()->tick());
		uint64_t targetFramesSent = elapsedTime / cycle;

                if (framesSent < targetFramesSent)
		{
                       OpenThreads::ScopedLock<OpenThreads::Mutex> lock(m_mutex);
			Write(m_frame[1]);
			framesSent++;
		}
		else
		{
                        OpenThreads::Thread::microSleep(10);
		}
	}

	// write the trailer, if any
	av_write_trailer(m_formatContext);
}

// Close the output video
void VideoStreamer::CloseVideo(void)
{

    //Check if allready open
        if(!_isOpen)
        return;
        printf("Closing Video\n");

	m_stoprequested = true;
//	if (m_thread)
        m_thread.join();

	if (m_vStream->codec->codec)
		avcodec_close(m_vStream->codec);
	av_free(m_vOutBuf);

	// Close the output file
	if (m_formatContext->pb && !(m_formatContext->oformat->flags & AVFMT_NOFILE))
        url_fclose(m_formatContext->pb); 

        ReleaseFrame(m_frame[0], false);
        ReleaseFrame(m_frame[1], false);
        ReleaseContext();
        _isOpen=false;
}



// Allocate and initialize format context
AVFormatContext* CreateFormatContext(
        const char *ai_fileName,
        const char *ai_shortName,
        const char *ai_fileExtension,
        const char *ai_mimeType,
        CodecID codec_id)
{
        AVFormatContext* formatContext =  avformat_alloc_context();
        if (!formatContext)
        {
                std::cerr << "ERROR: Could not allocate format context at ::CreateFormatContext()." << std::endl;
                return NULL;
        }
        formatContext->oformat = avformat_right_guess_version(ai_shortName, ai_fileExtension, ai_mimeType);

        if (!formatContext->oformat)
        {
                std::cerr << std::endl << "WARNING: Could not find suitable output format, using avi-mpeg4" << std::endl << std::endl;
                formatContext->oformat = avformat_right_guess_version("avi", NULL, NULL);
        }
        if (!formatContext->oformat)
        {
                std::cerr << "ERROR: Could not find suitable output format." << std::endl;
                return NULL;
        }
        else{
            formatContext->oformat->video_codec=codec_id;

        }
        sprintf(formatContext->filename, "%s", ai_fileName);

        return formatContext;
}

// Create a new video stream
AVStream *CreateVideoStream(
        AVFormatContext* ai_formatContext,
        unsigned int width,
        unsigned int height,
        int ai_bitRate,
        int ai_frameRate,
        int ai_gopSize,
        int ai_bFrames,
        const PixelFormat& ai_pixFmt)
{
        // Create the new video stream
        AVStream *st = av_new_stream(ai_formatContext, 0);
        if (!st)
        {
                std::cerr << "ERROR: Could not allocate the video stream at ::CreateVideoStream()." << std::endl;
                return NULL;
        }

        // Initialize the codec context
        AVCodecContext *c = st->codec;
        c->codec_id		  = ai_formatContext->oformat->video_codec;
        c->codec_type	  = AVMEDIA_TYPE_VIDEO;
        c->bit_rate		  = ai_bitRate;
        c->width		  = width;
        c->height		  = height;
        // time base: this is the fundamental unit of time (in seconds) in terms
        // of which frame timestamps are represented. for fixed-fps content,
        // timebase should be 1/framerate and timestamp increments should be
        // identically 1.
        c->time_base.num  = 1;
        c->time_base.den  = ai_frameRate;
        c->gop_size		  = ai_gopSize;
        c->pix_fmt		  = ai_pixFmt;


        if (c->codec_id == CODEC_ID_MPEG2VIDEO)
                c->max_b_frames = ai_bFrames; // we also add B frames
        if (c->codec_id == CODEC_ID_MPEG1VIDEO)
        {
                // needed to avoid using macroblocks in which some coeffs overflow
                // this doesnt happen with normal video, it just happens here as the
                // motion of the chroma plane doesnt match the luma plane
                c->mb_decision=2;
        }

        if(c->codec_id == CODEC_ID_H264){
            set_libx264Opt(c);
        }
        // Some formats want stream headers to be seperate
        const char *fn = ai_formatContext->oformat->name;
        if (!strcmp(fn, "mp4") || !strcmp(fn, "mov") || !strcmp(fn, "3gp"))
                c->flags |= CODEC_FLAG_GLOBAL_HEADER;


        return st;
}

// Allocate memory for a new frame
AVFrame *CreateFrame(int ai_pixFmt, int ai_width, int ai_height)
{
        // Alloc frame header
        AVFrame *frame = avcodec_alloc_frame();
        if (!frame)
                return NULL;

        // Alloc frame data
        uint8_t *data = (uint8_t*)av_malloc(avpicture_get_size((PixelFormat)ai_pixFmt, ai_width, ai_height));
        if (!data)
        {
                av_freep(frame);
                return NULL;
        }

        avpicture_fill((AVPicture *)frame, data, (PixelFormat)ai_pixFmt, ai_width, ai_height);

        return frame;
}

// Release memory from a AVFrame
void ReleaseFrame(AVFrame *ai_frame, bool ai_releaseData)
{
        if (ai_frame)
        {
                if (ai_releaseData && ai_frame->data[0])
                        av_freep(ai_frame->data[0]);

                av_freep(ai_frame);
        }
}

void set_libx264Opt(AVCodecContext *videoContext){

    // Set the libx264-specific options
    videoContext->me_cmp |= FF_CMP_CHROMA;                                                         // cmp=+chroma
    videoContext->partitions = X264_PART_I8X8 | X264_PART_I4X4 | X264_PART_P8X8 | X264_PART_B8X8;  // partitions=+parti8x8+parti4x4+partp8x8+partb8x8
    videoContext->me_method = 8;                                                                   // me_method=umh
    videoContext->me_subpel_quality = 8;                                                           // subq=8
    videoContext->me_range = 16;                                                                   // me_range=16
    videoContext->gop_size = 250;                                                                  // g=250
    videoContext->keyint_min = 25;                                                                 // keyint_min=25
    videoContext->scenechange_threshold = 40;                                                      // sc_threshold=40
    videoContext->i_quant_factor = 0.71f;                                                          // i_qfactor=0.71
    videoContext->b_frame_strategy = 2;                                                            // b_startegy=2
    videoContext->qcompress = 0.6f;                                                                // qcomp=0.6
    videoContext->qmin = 10;                                                                       // qmin = 10
    videoContext->qmax = 51;                                                                       //qmax = 51
    videoContext->max_qdiff = 4;                                                                   // qdiff=4
    videoContext->refs = 4;                                                                        //refs=4
    videoContext->directpred = 3;                                                                  //directpred=3
    videoContext->trellis = 1;                                                                     //trellis=1
    videoContext->flags2 |= CODEC_FLAG2_WPRED | CODEC_FLAG2_MIXED_REFS | CODEC_FLAG2_8X8DCT        // flags2=+wpred+mixed_refs+dct8x8+fastpskip
                            | CODEC_FLAG2_FASTPSKIP;

    videoContext->thread_count = 0; // let x264 decide the number of threads
    videoContext->bit_rate =      videoContext->bit_rate*100;
    videoContext->bit_rate_tolerance =      videoContext->bit_rate*100;


}
