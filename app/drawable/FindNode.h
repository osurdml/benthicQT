/* -*-c++-*- */
/* osgEarth - Dynamic map generation toolkit for OpenSceneGraph
* Copyright 2008-2010 Pelican Mapping
* http://osgearth.org
*
* osgEarth is free software; you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>
*/

#ifndef OSGEARTH_FINDNODE
#define OSGEARTH_FINDNODE 1


    /**
     * Visitor that locates a node by its type
     */
    template<typename T>
    class FindTopMostNodeOfTypeVisitor : public osg::NodeVisitor
    {
    public:
        FindTopMostNodeOfTypeVisitor():
          osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN),
              _foundNode(0)
          {}

          void apply(osg::Node& node)
          {
              T* result = dynamic_cast<T*>(&node);
              if (result)
              {
                  _foundNode = result;
              }
              else
              {
                  traverse(node);
              }
          }

          T* _foundNode;
    };

    /**
     * Searchs the scene graph downward starting at [node] and returns the first node found
     * that matches the template parameter type.
     */
    template<typename T>
    T* findTopMostNodeOfType(osg::Node* node)
    {
        if (!node) return 0;

        FindTopMostNodeOfTypeVisitor<T> fnotv;
        fnotv.setTraversalMode(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN);
        node->accept(fnotv);

        return fnotv._foundNode;
    }    

    /**
     * Searchs the scene graph upward starting at [node] and returns the first node found
     * that matches the template parameter type.
     */
    template<typename T>
    T* findFirstParentOfType(osg::Node* node)
    {
        if (!node) return 0;

        FindTopMostNodeOfTypeVisitor<T> fnotv;
        fnotv.setTraversalMode(osg::NodeVisitor::TRAVERSE_PARENTS);
        node->accept(fnotv);

        return fnotv._foundNode;
    }

    /**
     * Searchs the scene graph starting at [node] and returns the first node found
     * that matches the template parameter type. First searched upward, then downward.
     */
    template<typename T>
    T* findRelativeNodeOfType(osg::Node* node)
    {
        if ( !node ) return 0;
        T* result = findFirstParentOfType<T>( node );
        if ( !result )
            result = findTopMostNodeOfType<T>( node );
        return result;
    }

    /**
     * Adjusts a node's update traversal count by a delta.
     * Only safe to call from the UPDATE thread
     */
#define ADJUST_UPDATE_TRAV_COUNT( NODE, DELTA ) \
    { \
        int oldCount = NODE ->getNumChildrenRequiringUpdateTraversal(); \
        if ( oldCount + DELTA >= 0 ) \
            NODE ->setNumChildrenRequiringUpdateTraversal( (unsigned int)(oldCount + DELTA ) ); \
    }


#endif //OSGEARTH_FINDNODE
