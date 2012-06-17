// Copyright  (C)  2007  Ruben Smits <ruben dot smits at mech dot kuleuven dot be>

// Version: 1.0
// Author: Ruben Smits <ruben dot smits at mech dot kuleuven dot be>
// Maintainer: Ruben Smits <ruben dot smits at mech dot kuleuven dot be>
// URL: http://www.orocos.org/kdl

// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.

// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.

// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

#ifndef KDL_TREE_HPP
#define KDL_TREE_HPP

#include "segment.hpp"
#include "chain.hpp"

#include <string>
#include <map>
#if !defined(__APPLE__) && !defined(MAC_OS_X_VERSION_10_5)
#include <Eigen/Core>
#endif

namespace KDL
{
    //Forward declaration
    class TreeElement;
#if !defined(__APPLE__) && !defined(MAC_OS_X_VERSION_10_5)
    // Eigen allocator is needed for alignment of Eigen data types
    typedef std::map<std::string,TreeElement, std::less<std::string>, Eigen::aligned_allocator<std::pair<std::string, TreeElement> > > SegmentMap;
#else
    typedef std::map<std::string,TreeElement> SegmentMap;
#endif
    class TreeElement
    {
    public:
        TreeElement():q_nr(0)
        {};
    public:
        Segment segment;
        unsigned int q_nr;
        SegmentMap::const_iterator  parent;
        std::vector<SegmentMap::const_iterator > children;
        TreeElement(const Segment& segment_in,const SegmentMap::const_iterator& parent_in,unsigned int q_nr_in)
        {
			q_nr=q_nr_in;
            segment=segment_in;
            parent=parent_in;
        };
        static TreeElement Root()
        {
            return TreeElement();
        };
    };

    /**
     * \brief  This class encapsulates a <strong>tree</strong>
     * kinematic interconnection structure. It is build out of segments.
     *
     * @ingroup KinematicFamily
     */
    class Tree
    {
    private:
        SegmentMap segments;
        unsigned int nrOfJoints;
        unsigned int nrOfSegments;

        bool addTreeRecursive(SegmentMap::const_iterator root, const std::string& tree_name, const std::string& hook_name);

    public:
        /**
         * The constructor of a tree, a new tree is always empty
         */
        Tree();
        Tree(const Tree& in);
        Tree& operator= (const Tree& arg);

        /**
         * Adds a new segment to the end of the segment with
         * hook_name as segment_name
         *
         * @param segment new segment to add
         * @param segment_name name of the new segment
         * @param hook_name name of the segment to connect this
         * segment with.
         *
         * @return false if hook_name could not be found.
         */
        bool addSegment(const Segment& segment, const std::string& segment_name, const std::string& hook_name);

        /**
         * Adds a complete chain to the end of the segment with
         * hook_name as segment_name. Segment i of
         * the chain will get chain_name+".Segment"+i as segment_name.
         *
         * @param chain Chain to add
         * @param chain_name name of the chain
         * @param hook_name name of the segment to connect the chain with.
         *
         * @return false if hook_name could not be found.
         */
        bool addChain(const Chain& chain, const std::string& chain_name, const std::string& hook_name);

        /**
         * Adds a complete tree to the end of the segment with
         * hookname as segment_name. The segments of the tree will get
         * tree_name+segment_name as segment_name.
         *
         * @param tree Tree to add
         * @param tree_name name of the tree
         * @param hook_name name of the segment to connect the tree with
         *
         * @return false if hook_name could not be found
         */
        bool addTree(const Tree& tree, const std::string& tree_name,const std::string& hook_name);

        /**
         * Request the total number of joints in the tree.\n
         * <strong> Important:</strong> It is not the same as the
         * total number of segments since a segment does not need to have
         * a joint.
         *
         * @return total nr of joints
         */
        unsigned int getNrOfJoints()const
        {
            return nrOfJoints;
        };

        /**
         * Request the total number of segments in the tree.
         * @return total number of segments
         */
        unsigned int getNrOfSegments()const {return nrOfSegments;};

        /**
         * Request the segment of the tree with name segment_name.
         *
         * @param segment_name the name of the requested segment
         *
         * @return constant iterator pointing to the requested segment
         */
        SegmentMap::const_iterator getSegment(const std::string& segment_name)const
        {
            return segments.find(segment_name);
        };



        const SegmentMap& getSegments()const
        {
            return segments;
        }

        virtual ~Tree(){};
    };
}
#endif





