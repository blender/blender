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

#include "tree.hpp"
#include <sstream>
namespace KDL {
using namespace std;

Tree::Tree() :
    nrOfJoints(0), nrOfSegments(0) {
    segments.insert(make_pair("root", TreeElement::Root()));
}

Tree::Tree(const Tree& in) {
    segments.clear();
    nrOfSegments = 0;
    nrOfJoints = 0;

    segments.insert(make_pair("root", TreeElement::Root()));
    this->addTree(in, "", "root");

}

Tree& Tree::operator=(const Tree& in) {
    segments.clear();
    nrOfSegments = 0;
    nrOfJoints = 0;

    segments.insert(make_pair("root", TreeElement::Root()));
    this->addTree(in, "", "root");
    return *this;
}

bool Tree::addSegment(const Segment& segment, const std::string& segment_name,
        const std::string& hook_name) {
    SegmentMap::iterator parent = segments.find(hook_name);
    //check if parent exists
    if (parent == segments.end())
        return false;
    pair<SegmentMap::iterator, bool> retval;
    //insert new element
    retval = segments.insert(make_pair(segment_name, TreeElement(segment,
            parent, nrOfJoints)));
    //check if insertion succeeded
    if (!retval.second)
        return false;
    //add iterator to new element in parents children list
    parent->second.children.push_back(retval.first);
    //increase number of segments
    nrOfSegments++;
    //increase number of joints
	nrOfJoints += segment.getJoint().getNDof();
    return true;
}

bool Tree::addChain(const Chain& chain, const std::string& chain_name,
        const std::string& hook_name) {
    string parent_name = hook_name;
    for (unsigned int i = 0; i < chain.getNrOfSegments(); i++) {
        ostringstream segment_name;
        segment_name << chain_name << "Segment" << i;
        if (this->addSegment(chain.getSegment(i), segment_name.str(),
                parent_name))
            parent_name = segment_name.str();
        else
            return false;
    }
    return true;
}

bool Tree::addTree(const Tree& tree, const std::string& tree_name,
        const std::string& hook_name) {
    return this->addTreeRecursive(tree.getSegment("root"), tree_name, hook_name);
}

bool Tree::addTreeRecursive(SegmentMap::const_iterator root,
        const std::string& tree_name, const std::string& hook_name) {
    //get iterator for root-segment
    SegmentMap::const_iterator child;
    //try to add all of root's children
    for (unsigned int i = 0; i < root->second.children.size(); i++) {
        child = root->second.children[i];
        //Try to add the child
        if (this->addSegment(child->second.segment, tree_name + child->first,
                hook_name)) {
            //if child is added, add all the child's children
            if (!(this->addTreeRecursive(child, tree_name, tree_name
                    + child->first)))
                //if it didn't work, return false
                return false;
        } else
            //If the child could not be added, return false
            return false;
    }
    return true;
}

}

