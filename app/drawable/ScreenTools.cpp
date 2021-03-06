#include "ScreenTools.h"

#include <cstring>

// class to handle events with a pick
using namespace osgGA;
osg::Geode* measure_geode=NULL;
bool measuring_tool_on=false;
bool oldmesh=false;

bool PickHandler::handle(const osgGA::GUIEventAdapter& ea,osgGA::GUIActionAdapter& aa)
{
    switch(ea.getEventType())
    { 

    case(osgGA::GUIEventAdapter::DOUBLECLICK):
    {
        if(measuring_tool_on){
            // if(ea.getButtonMask() ==GUIEventAdapter::MIDDLE_MOUSE_BUTTON){
            osgViewer::Viewer* viewer = dynamic_cast<osgViewer::Viewer*>(&aa);
            if (viewer) pick(viewer,ea);
            //      }
        }
        return false;
    }    
    case(osgGA::GUIEventAdapter::MOVE):
      {
	if(!measure_anchored)
	  return false;
      osgViewer::Viewer* viewer = dynamic_cast<osgViewer::Viewer*>(&aa);
	if (viewer ) pick_measure(viewer,ea);
	return false;
      }
 default:
      return false;
    }
}
osgText::Text* createMeasureText(osg::Vec3 start, osg::Vec3 end)
{

 
    //text->setCullCallback(new osg::ClusterCullingCallback(pos, normal, 0.0));
  osgText::Text* text = new osgText::Text();
  text->setText("");
  text->setFont(osgDB::findDataFile("osgsightdata/arial.ttf"));
	       

 text->setCharacterSize(300000.0f);
 text->setCharacterSizeMode(osgText::Text::OBJECT_COORDS_WITH_MAXIMUM_SCREEN_SIZE_CAPPED_BY_FONT_HEIGHT);
 text->setAutoRotateToScreen(true);
 
 return text;
}

void PickHandler::pick_measure(osgViewer::Viewer* viewer, const osgGA::GUIEventAdapter& ea)
{

  
  
  osgUtil::LineSegmentIntersector::Intersections intersections;
  if (viewer->computeIntersections(ea.getX(), ea.getY(), intersections)){
    
    // use the nearest intersection                 
    const osgUtil::LineSegmentIntersector::Intersection& intersection = *(intersections.begin());
  
    (*measure_vertices)[1].set( intersection.getWorldIntersectPoint());
    // cout <<   (*measure_vertices)[1] <<endl;
}

}

void PickHandler::pick(osgViewer::Viewer* viewer, const osgGA::GUIEventAdapter& ea)
{


    
    std::string gdlist="";
    std::ostringstream os;
      
    osgUtil::LineSegmentIntersector::Intersections intersections;
    if (viewer->computeIntersections(ea.getX(), ea.getY(), intersections)){
      
      // use the nearest intersection                 
      const osgUtil::LineSegmentIntersector::Intersection& intersection = *(intersections.begin());
      if(measuring_tool_on){
	if(!measure_anchored){
	  if(!measure_added){
	    group->addChild(measure_geode);
	    measure_added=true;
	  }
	  text_ptr->setText("");
	  //	printf("Anchor\n");
	  (*measure_vertices)[0].set( intersection.getWorldIntersectPoint());
	  (*measure_vertices)[1].set( intersection.getWorldIntersectPoint()+osg::Vec3(0,1.0,0.0));
	  
	  measure_anchored=true;
	}else{
	  (*measure_vertices)[1].set( intersection.getWorldIntersectPoint());
	  //	printf("Complete\n");
	  osg::Vec3 start=(*measure_vertices)[0];
	  osg::Vec3 end=(*measure_vertices)[1];
	  
	  osg::Vec3 diff=end-start;
	  double dist=sqrt(pow(diff[0],2)+pow(diff[1],2)+pow(diff[2],2));
	  
	  char text_str[255];
          sprintf(text_str,"Dist: %.2f\nX: %.2f\nY: %.2f\nZ: %.2f\n",dist,diff[0],diff[2],diff[1]);
	  text_ptr->setText(string(text_str));
	  text_ptr->setPosition(end+osg::Vec3(0,-0.5,0));
	  measure_anchored=false;
          osg::Vec3 distV(dist,dist,dist);
          _mf->measureResultsEmit(distV,diff);
	}
      }
      if(ea.getButtonMask() ==GUIEventAdapter::MIDDLE_MOUSE_BUTTON){
      osg::Drawable* drawable = intersection.drawable.get();
      osg::Geometry* geometry = drawable ? drawable->asGeometry() : 0;
      
      osg::Vec3Array* vertices = geometry ? dynamic_cast<osg::Vec3Array*>(geometry->getVertexArray()) : 0;
      if (vertices){
	
	// get the vertex indices.
	const osgUtil::LineSegmentIntersector::Intersection::IndexList& indices = intersection.indexList;
	const osgUtil::LineSegmentIntersector::Intersection::RatioList& ratios = intersection.ratioList;
	
	if (indices.size()==3 && ratios.size()==3){
	  bool textured = false;
	  osg::StateSet* stateset = drawable->getStateSet();
	  if(stateset){
	    textured=(stateset->getTextureMode(0,GL_TEXTURE_1D)) || 
	      (stateset->getTextureMode(0,GL_TEXTURE_2D)) || 
		      (stateset->getTextureMode(0,GL_TEXTURE_3D)); 
	  }
	  if(textured){

	    osg::Texture* texture = dynamic_cast<osg::Texture*>(stateset->getTextureAttribute(0,osg::StateAttribute::TEXTURE));
	 
	    osg::Texture2D* texture2D = dynamic_cast<osg::Texture2D*>(texture);

/*	    if(texture && texshow){
	      texshow->show(texture);
            }*/
	  unsigned int i1 = indices[0];
	  unsigned int i2 = indices[1];
	  unsigned int i3 = indices[2];
	 
	  float r1 = ratios[0];
	  float r2 = ratios[1];
	  float r3 = ratios[2];
	  
	  osg::Array* texcoords = (geometry->getNumTexCoordArrays()>0) ? geometry->getTexCoordArray(0) : 0;
	  
	  osg::Vec2Array* texcoords_Vec2Array = dynamic_cast<osg::Vec2Array*>(texcoords);
	  if (texcoords_Vec2Array){
	    // we have tex coord array so now we can compute the final tex coord at the point of intersection.                                
	    osg::Vec2 tc1 = (*texcoords_Vec2Array)[i1];
	    osg::Vec2 tc2 = (*texcoords_Vec2Array)[i2];
	    osg::Vec2 tc3 = (*texcoords_Vec2Array)[i3];
	    osg::Vec2 tc = tc1*r1 + tc2*r2 + tc3*r3;
	    cout << "We hit tex coords "<<tc << " " <<std::endl;
	    osg::Vec2 tc_noatlas = (tc / 4.0) ;//- osg::Vec2(fmod(tc[0],4.0f),fmod(tc[1],4.0f)) ;
	    cout << tc_noatlas<<endl;
	    /*    osg::notify(osg::NOTICE)<<"We hit tex coords "<<tc << " " << name <<std::endl;
	    os << "We hit tex coords "<<tc << " " << name <<std::endl;
	    cout << "We hit tex coords "<<tc << " " << name <<std::endl;*/
	    gdlist += os.str();
	    //winPtr->loadImage(QString(name.c_str()));
	  }
	  
	  }
	}
      }
    
    else {
      osg::notify(osg::NOTICE)<<"Intersection has insufficient indices to work with";
	}
	
      }
    }
    else{
      
      osg::notify(osg::NOTICE)<<"No intersection"<<std::endl;
    }

    /*if (_viewer->computeIntersections(ea.getX(),ea.getY(),hlist))
    {
    for(osgUtil::IntersectVisitor::HitList::iterator hitr=hlist.begin();
    hitr!=hlist.end();
    ++hitr)
    {
            
    if (hitr->_geode.valid() && !hitr->_geode->getName().empty())
    {
    // the geodes are identified by name.
    os<<"Object \""<<hitr->_geode->getName()<<"\""<<std::endl;
    }
    else if (hitr->_drawable.valid())
    {
    os<<"Object \""<<hitr->_drawable->className()<<"\""<<std::endl;
    }
    
            os<<"        local coords vertex("<< hitr->getLocalIntersectPoint()<<")"<<"  normal("<<hitr->getLocalIntersectNormal()<<")"<<std::endl;
            os<<"        world coords vertex("<< hitr->getWorldIntersectPoint()<<")"<<"  normal("<<hitr->getWorldIntersectNormal()<<")"<<std::endl;
            osgUtil::Hit::VecIndexList& vil = hitr->_vecIndexList;
            for(unsigned int i=0;i<vil.size();++i)
            {
	    os<<"        vertex indices ["<<i<<"] = "<<vil[i]<<std::endl;
            }
            
            gdlist += os.str();
	    }
	    }*/
    //setLabel(gdlist);
}

osg::Camera* createBox(int xpos, int ypos,int hud_width,int hud_height){
  osg::Camera* quad_cam = new osg::Camera;
 
  
  
  quad_cam->setViewport(xpos,ypos,hud_width,hud_height);
  // set up the background color and clear mask.
  quad_cam->setClearColor(osg::Vec4(0.1f,0.1f,0.3f,1.0f));
  quad_cam->setClearMask(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  
  // the quad_cam is going to look at our input quad
  quad_cam->setProjectionMatrix(osg::Matrix::ortho2D(0,1,0,1));
  quad_cam->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
  quad_cam->setViewMatrix(osg::Matrix::identity());
  quad_cam->setRenderOrder(osg::Camera::POST_RENDER);
  return quad_cam;
}

osg::Group* createRTTQuad(osg::Texture *texture)
{
    osg::Group *top_group = new osg::Group;
    
    osg::ref_ptr<osg::Geode> quad_geode = new osg::Geode;

    osg::ref_ptr<osg::Vec3Array> quad_coords = new osg::Vec3Array; // vertex coords
    // counter-clockwise
    quad_coords->push_back(osg::Vec3d(0, 0, -1));
    quad_coords->push_back(osg::Vec3d(1, 0, -1));
    quad_coords->push_back(osg::Vec3d(1, 1, -1));
    quad_coords->push_back(osg::Vec3d(0, 1, -1));

    osg::ref_ptr<osg::Vec2Array> quad_tcoords = new osg::Vec2Array; // texture coords
    quad_tcoords->push_back(osg::Vec2(0, 0));
    quad_tcoords->push_back(osg::Vec2(1.0, 0));
    quad_tcoords->push_back(osg::Vec2(1.0, 1.0));
    quad_tcoords->push_back(osg::Vec2(0, 1.0));

    osg::ref_ptr<osg::Geometry> quad_geom = new osg::Geometry;
    osg::ref_ptr<osg::DrawArrays> quad_da = new osg::DrawArrays(osg::PrimitiveSet::QUADS,0,4);

    osg::ref_ptr<osg::Vec4Array> quad_colors = new osg::Vec4Array;
    quad_colors->push_back(osg::Vec4(1.0f,1.0f,1.0f,1.0f));

    quad_geom->setVertexArray(quad_coords.get());
    quad_geom->setTexCoordArray(0, quad_tcoords.get());
    quad_geom->addPrimitiveSet(quad_da.get());
    quad_geom->setColorArray(quad_colors.get());
    quad_geom->setColorBinding(osg::Geometry::BIND_OVERALL);
    
    osg::StateSet *stateset = quad_geom->getOrCreateStateSet();
    stateset->setMode(GL_LIGHTING,osg::StateAttribute::OFF);
    if(texture)
      stateset->setTextureAttributeAndModes(0, texture,osg::StateAttribute::ON);
    quad_geode->addDrawable(quad_geom.get());
    
    top_group->addChild(quad_geode.get());

    return top_group;
}
struct MyCameraPostDrawCallback : public osg::Camera::DrawCallback
{
  MyCameraPostDrawCallback(osg::Node *to_clear): to_clear(to_clear)
    {
    }

    virtual void operator () (const osg::Camera&) const
    {
      to_clear->setNodeMask(0x0);

    }
  osg::Node *to_clear;

};
void Transform_Point(double out[4], const double m[16], const double in[4])
{
#define M(row,col)  m[col*4+row]
        out[0] =
                M(0, 0) * in[0] + M(0, 1) * in[1] + M(0, 2) * in[2] + M(0, 3) * in[3];
        out[1] =
                M(1, 0) * in[0] + M(1, 1) * in[1] + M(1, 2) * in[2] + M(1, 3) * in[3];
        out[2] =
                M(2, 0) * in[0] + M(2, 1) * in[1] + M(2, 2) * in[2] + M(2, 3) * in[3];
        out[3] =
                M(3, 0) * in[0] + M(3, 1) * in[1] + M(3, 2) * in[2] + M(3, 3) * in[3];
#undef M
}
osg::Vec3d WorldToScreen(osg::Camera *cam,osg::Vec3 worldpoint)
{
        double in[4], out[4];

        in[0] = worldpoint._v[0];
        in[1] = worldpoint._v[1];
        in[2] = worldpoint._v[2];
        in[3] = 1.0;

        osg::Matrix projectMatrix= cam->getProjectionMatrix();
        osg::Matrix viewprojectMatrix = cam->getViewMatrix();

        double modelViewMatrix[16];
        memcpy(modelViewMatrix,viewprojectMatrix.ptr(),sizeof(GLdouble) * 16);
        Transform_Point(out, modelViewMatrix, in);


        double myprojectMatrix[16];
        memcpy(myprojectMatrix,projectMatrix.ptr(),sizeof(GLdouble) * 16);

        Transform_Point(in, myprojectMatrix, out);

        if (in[3] == 0.0)
                return osg::Vec3d(0,0,0);

        in[0] /= in[3];
        in[1] /= in[3];
        in[2] /= in[3];

        int viewPort[4];
        osg::Viewport* myviewPort = cam->getViewport();
        viewPort[0] = myviewPort->x();
        viewPort[1] = myviewPort->y();
        viewPort[2] = myviewPort->width();
        viewPort[3] = myviewPort->height();

        osg::Vec3d sceenPoint;
        sceenPoint._v[0] = (int)(viewPort[0] + (1 + in[0]) * viewPort[2] / 2 + 0.5);
        sceenPoint._v[1] = (int)(viewPort[1] + (1 + in[1]) * viewPort[3] / 2 + 0.5);
        sceenPoint._v[2]  = 0;
        return sceenPoint;
}



class mapSquareUpdate : public osg::NodeCallback 
{
public:
  mapSquareUpdate(WorldWindManipulatorNew *om,osg::Camera *cam,int tex_width,int tex_height):om(om),cam(cam),tex_width(tex_width),tex_height( tex_height) {
  
  }
  virtual void operator()(osg::Node* node, osg::NodeVisitor* nv)
  {
    osg::MatrixTransform *tx = dynamic_cast<osg::MatrixTransform *>(node);
    if( tx != NULL )
      {

        osg::Vec3 	center=  om->getCenter();
        osg::Vec3 screen =  WorldToScreen(cam,center);
        osg::Vec3 tranV(screen[0]/tex_width,screen[1]/tex_height,0);

        double scale = (om->getDistance()/om->getHomeDist());
        if(scale < 0.05)
          scale = 0.05;
        tx->setMatrix(osg::Matrix::scale(scale,scale,1.0)*osg::Matrix::translate(tranV));
      }
    traverse(node, nv);
  }
private:
  osg::Vec3 pose;
  osg::Camera *cam;
  WorldWindManipulatorNew *om;
  int tex_width,tex_height;
};

class scaleBarUpdate : public osg::NodeCallback
{
public:
    scaleBarUpdate(WorldWindManipulatorNew *om,osg::Camera *cam,osgText::Text *text,osg::Vec3Array *verts):om(om),cam(cam),text(text),verts(verts) {

  }
  virtual void operator()(osg::Node* node, osg::NodeVisitor* nv)
  {
    osg::MatrixTransform *tx = dynamic_cast<osg::MatrixTransform *>(node);
    if( tx != NULL )
      {
double height =10;
double width =150;
double offset =15.0;
        double fovy,aspectratio,znear,zfar;
        cam->getProjectionMatrixAsPerspective(fovy,aspectratio,znear,zfar);
       // printf("f %f a %f zn %f zf %f",fovy,aspectratio,znear,zfar);
        //osg::Vec3f eye; osg::Vec3f center; osg::Vec3f up;
       // cam->getViewMatrixAsLookAt(eye,center,up);
        //cout << "eye "<<eye<<endl;
        //cout <<center <<endl;
        //printf("fov %f %f %f %f\n",fovy,aspectratio,znear,zfar);
        double fovx = atan(tan(osg::DegreesToRadians(fovy*0.5)) * aspectratio) * 2.0;

        const osg::Viewport* viewport    = cam->getViewport();
        double vpwidth  = viewport->width();

        double vpheight  = viewport->height();
	//double fovx=cam->calc_fovx();
	//	double fovy2=cam->calc_fovy();
	//	printf("%f %f %f\n",fovx, fovy,fovy2);
        double pixelSize=computePixelSizeAtDistance(om->getDistance(),fovy,vpheight);
       double scaleSize = pixelSize * width ;  // meter
                       string unitLabel = "m";

                           if (scaleSize > 10000)
                           {
                               scaleSize /= 1000;
                               unitLabel = "Km";
                           }
                           else if (scaleSize < 1.0 )
                           {
                               scaleSize *= 100;
                               unitLabel = "cm";
                           }

                       // Rounded division size
                       int pot = (int) floor(log10(scaleSize));
                       if (!isnan(pot))
                       {
                           char tmp[1024];
                           sprintf(tmp,"%.0f", scaleSize);
                                   string a=string(tmp).substr(0, 1);
                           int digit = atoi(a.c_str());
                           double divSize = digit * pow(10.0, pot);
                           if (digit >= 5)
                               divSize = 5 * pow(10.0, pot);
                           else if (digit >= 2)
                               divSize = 2 * pow(10.0, pot);
                           double divWidth = width * divSize / scaleSize;
                 //      printf("Pixel Distance %f %f %f\n",pixelSize,divSize,divWidth);
                       const double offsetX=(width - divWidth) / 2;
                       verts->at(0)=osg::Vec3(0+offsetX, height, 0);
                       verts->at(1)=osg::Vec3(0+offsetX, 0, 0);

                             verts->at(2)=osg::Vec3(divWidth+offsetX, 0, 0);
                             verts->at(3)=osg::Vec3(divWidth+offsetX, height, 0);

                            verts->at(4)=osg::Vec3((divWidth / 2)+offsetX, 0, 0);
                             verts->at(5)=osg::Vec3((divWidth / 2)+offsetX, height / 2, 0);
                             char label[8129];
                             sprintf(label,"%.0f %s", divSize, unitLabel.c_str());
                              text->setText(label);
                              float scale =1.0;
                              text->setPosition(osg::Vec3((divWidth * scale / 2 + (width - divWidth) / 2)-offset, height*scale,0.0));
}


      }
    traverse(node, nv);
  }
private:
  osg::Vec3 pose;
  osg::Camera *cam;
  WorldWindManipulatorNew *om;
  osgText::Text *text;
  osg::Vec3Array *verts;
};
void addMapSqaure(osg::Group *mapGroup,WorldWindManipulatorNew *om,osg::Camera *cam,int tex_width,int tex_height){
  osg::Geode* geode = new osg::Geode();
  osg::Geometry* polyGeom = new osg::Geometry();
  osg::ref_ptr<osg::Vec4Array> shared_colors = new osg::Vec4Array;
  shared_colors->push_back(osg::Vec4(1.0f,0.0f,0.0f,1.0f));
   
  // same trick for shared normal.
  osg::ref_ptr<osg::Vec3Array> shared_normals = new osg::Vec3Array;
  shared_normals->push_back(osg::Vec3(0.0f,-1.0f,0.0f));
  osg::Vec3 myCoords[] ={
   
    osg::Vec3(-0.49f, -0.49f, 0.0),
    osg::Vec3(-0.49f, 0.49f,0.0),
    osg::Vec3(0.49, 0.49f, 0.0),
    osg::Vec3(0.49, -0.49f, 0.0),

  };
   
  int numCoords = sizeof(myCoords)/sizeof(osg::Vec3);
	  
  osg::Vec3Array* vertices = new osg::Vec3Array(numCoords,myCoords);
   
  // pass the created vertex array to the points geometry object.
  polyGeom->setVertexArray(vertices);
  osg::PolygonMode *polygonMode = new osg::PolygonMode;	  
  polygonMode->setMode(osg::PolygonMode::FRONT_AND_BACK, 
		       osg::PolygonMode::LINE);
  osg::StateSet *stateSet = polyGeom->getOrCreateStateSet();
  stateSet->setAttributeAndModes(polygonMode, osg::StateAttribute::ON);
  // use the shared color array.
  polyGeom->setColorArray(shared_colors.get());
  polyGeom->setColorBinding(osg::Geometry::BIND_OVERALL);
   

  // use the shared normal array.
  polyGeom->setNormalArray(shared_normals.get());
  polyGeom->setNormalBinding(osg::Geometry::BIND_OVERALL);
   
   
  // This time we simply use primitive, and hardwire the number 
  //of coords to use 
  // since we know up front,
  polyGeom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::POLYGON,
						0,numCoords));
	   
   
  // add the points geomtry to the geode.
  geode->addDrawable(polyGeom);
  osg::MatrixTransform * transgeode = new osg::MatrixTransform();
  transgeode ->setUpdateCallback( new mapSquareUpdate(om,cam,tex_width,
						      tex_height) );
  transgeode ->addChild( geode);
  mapGroup->addChild(transgeode);
}
osg::Camera* createOrthoView(osg::Node* subgraph, const osg::Vec4& clearColour, WorldWindManipulatorNew *om,int screen_width,int screen_height,int hud_width,int hud_height,int hud_margin){
  osg::Texture* texture = 0;
  unsigned int samples = 0;
  unsigned int colorSamples = 0;  
   
  int tex_width=512;
  int tex_height=512;

  {  
    osg::Texture2D* texture2D = new osg::Texture2D;
    texture2D->setTextureSize(tex_width, tex_height);
    texture2D->setInternalFormat(GL_RGBA);
    texture2D->setFilter(osg::Texture2D::MIN_FILTER,osg::Texture2D::LINEAR);
    texture2D->setFilter(osg::Texture2D::MAG_FILTER,osg::Texture2D::LINEAR);
    
    texture = texture2D;
  }
  
  osg::CameraNode* camera = new osg::CameraNode;
  
  // set the viewport
  camera->setViewport(0,0,tex_width,tex_height);
  camera->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
  //camera->setTransformOrder(osg::CameraNode::POST_MULTIPLY);
  camera->setRenderTargetImplementation( osg::Camera::FRAME_BUFFER_OBJECT);
  camera->setPostDrawCallback(new MyCameraPostDrawCallback(camera)) ;
  
  const osg::BoundingSphere& bs = subgraph->getBound();
  osg::Matrix viewMatrix;
  if(oldmesh){
      viewMatrix.makeLookAt(bs.center()-osg::Vec3(0.0,2.0f*bs.radius(),0.0),
                        bs.center(),osg::Vec3(0.0f,0.0f,1.0f));
  }else
      viewMatrix.makeLookAt(bs.center()+osg::Vec3(0.0f,-1.0f,-3.5f)*bs.radius(),bs.center(),osg::Vec3(0.0f,0.0f,1.0f));

  //viewMatrix.makeLookAt(bs.center()+osg::Vec3(0.0,0.0,3.5f*bs.radius()),
        //		bs.center(),osg::Vec3(0.0f,0.0f,1.0f));
  if(oldmesh){
      camera->setProjectionMatrixAsOrtho(-(bs.radius()+bs.radius()/2),
				     bs.radius()+bs.radius()/2,
                                     -bs.radius(), bs.radius(),0, 0);
  }else
     camera->setProjectionMatrixAsOrtho2D(-bs.radius(),bs.radius(),-bs.radius(),bs.radius());

  camera->setViewMatrix(viewMatrix);
  // set clear the color and depth buffer
  camera->setClearColor(clearColour);
  camera->setClearMask(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

  // draw subgraph after main camera view.
  camera->setRenderOrder(osg::CameraNode::PRE_RENDER);
  
  // add the subgraph to draw.
  camera->addChild(subgraph);
  //
  osg::StateSet* stateset =  camera->getOrCreateStateSet();
  stateset->setAttribute(new osg::FrontFace(osg::FrontFace::CLOCKWISE));
  stateset->setMode(GL_LIGHTING,
		    osg::StateAttribute::OVERRIDE|osg::StateAttribute::OFF); 
  
  camera->setStateSet(stateset);
  // attach the texture and use it as the color buffer.
  camera->attach(osg::Camera::COLOR_BUFFER, texture, 
		 0, 0, false,
		 samples, colorSamples);
// now create the camera to do the multiple render to texture
  int w = screen_width, h = screen_height;
 double _mod;
  if(w < 1.5*hud_width ||h < 1.5*hud_height){
      _mod=0.5;
  }else{
      _mod=1.0;
     }


      int hw=hud_width*_mod;
      int hh=hud_height*_mod;

 // _mapCam->setViewport(w -_hud_margin-hw,
   ///                    h -_hud_margin-hh,
      //                 hw,hh);
  osg::Camera* quad_cam = createBox(w -hud_margin-hw,
                           h -hud_margin-hh,
                           hw,hh);/*createBox(screen_width -hud_margin-hud_width,
                    screen_height -hud_margin-hud_height,
                    hud_width,hud_height);*/
  quad_cam->setDataVariance(osg::Object::DYNAMIC);
  addMapSqaure(quad_cam,om, camera,tex_width,tex_height);

  osg::Node *map_quad=createRTTQuad(texture);
  quad_cam->addChild(camera);
  quad_cam->addChild(map_quad);
  
  return quad_cam;
}

 double computePixelSizeAtDistance(double distance, double fieldOfView, double viewportWidth)

{
    // printf("%f %f %f\n",distance,fieldOfView,viewportWidth);
  double  TanHalfAngle = tan(osg::DegreesToRadians(fieldOfView)*0.5);



    // If the viewport width is zero, than replace it with 1, which effectively ignores the viewport width.


    double pixelSizeScale = 2 * TanHalfAngle / (viewportWidth <= 0 ? 1.0 : viewportWidth);


    return fabs(distance) * pixelSizeScale;

}
 osg::Node *createScaleBar(osgText::Text *textNode,WorldWindManipulatorNew *om,osg::Camera *cam){

     osg::Group * transgeode = new osg::MatrixTransform();
     osg::Geode* geode  = new osg::Geode;
     geode->addDrawable(textNode);
     geode->setDataVariance(osg::Object::DYNAMIC);
     osg::ref_ptr<osg::Geometry> scalebarGeometry = new osg::Geometry();

     osg::ref_ptr<osg::DrawElementsUInt> scalebarBase = new osg::DrawElementsUInt(osg::PrimitiveSet::LINE_STRIP, 0);
     scalebarBase->push_back(0);
     scalebarBase->push_back(1);
     scalebarBase->push_back(2);
     scalebarBase->push_back(3);
     scalebarGeometry->addPrimitiveSet(scalebarBase.get());
     osg::ref_ptr<osg::DrawElementsUInt> scalebarBase2 = new osg::DrawElementsUInt(osg::PrimitiveSet::LINE_STRIP, 0);
     scalebarBase2->push_back(4);
     scalebarBase2->push_back(5);
     osg::Vec3Array *verts=new osg::Vec3Array(6);
     for(int i=0; i<verts->size(); i++){
         verts->at(i)=osg::Vec3(0,0,0);
     }
     scalebarGeometry->setVertexArray(verts);
     scalebarGeometry->addPrimitiveSet(scalebarBase2.get());
     scalebarGeometry->setUseDisplayList(false);
     osg::Vec4Array *carray=new osg::Vec4Array;
     carray->push_back(osg::Vec4(1.0,1.0,1.0,1.0));
     scalebarGeometry->setColorArray(carray);
     scalebarGeometry->setColorBinding(osg::Geometry::BIND_OVERALL);
     osg::Vec3Array* HUDnormals = new osg::Vec3Array;
            HUDnormals->push_back(osg::Vec3(0.0f,0.0f,1.0f));
            scalebarGeometry->setNormalArray(HUDnormals);
            scalebarGeometry->setNormalBinding(osg::Geometry::BIND_OVERALL);
            osg::StateSet* state = geode->getOrCreateStateSet();

             state->setMode(GL_LIGHTING, osg::StateAttribute::OFF);

     geode->addDrawable(scalebarGeometry);
     osg::MatrixTransform *mt=new osg::MatrixTransform;
     mt->setMatrix(osg::Matrix::translate(40,20,0));
     mt->addChild(geode);
     transgeode->addChild(mt);
     transgeode ->setUpdateCallback( new scaleBarUpdate(om,cam,textNode,verts) );
    return transgeode;
 }
