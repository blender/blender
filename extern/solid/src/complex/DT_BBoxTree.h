/*
 * SOLID - Software Library for Interference Detection
 * 
 * Copyright (C) 2001-2003  Dtecta.  All rights reserved.
 *
 * This library may be distributed under the terms of the Q Public License
 * (QPL) as defined by Trolltech AS of Norway and appearing in the file
 * LICENSE.QPL included in the packaging of this file.
 *
 * This library may be distributed and/or modified under the terms of the
 * GNU General Public License (GPL) version 2 as published by the Free Software
 * Foundation and appearing in the file LICENSE.GPL included in the
 * packaging of this file.
 *
 * This library is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
 * WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Commercial use or any other use of this library not covered by either 
 * the QPL or the GPL requires an additional license from Dtecta. 
 * Please contact info@dtecta.com for enquiries about the terms of commercial
 * use of this library.
 */

#ifndef DT_BBOXTREE_H
#define DT_BBOXTREE_H

#include <new>
#include <algorithm>

#include "DT_Convex.h"
#include "DT_CBox.h"


class DT_BBoxTree {
public:
    enum NodeType { INTERNAL = 0, LEAF = 1 };
    
    DT_BBoxTree() {}
    DT_BBoxTree(const DT_CBox& cbox, DT_Index index, NodeType type) 
      : m_cbox(cbox),
        m_index(index),
        m_type(type)
    {}
    
    DT_CBox  m_cbox;
    DT_Index m_index;
    NodeType m_type;
};



class DT_BBoxNode {
public:
    DT_BBoxNode() {}    
    DT_BBoxNode(int first, int last, int& node, DT_BBoxNode *free_nodes, const DT_CBox *boxes, DT_Index *indices, const DT_CBox& bbox);

    void makeChildren(DT_BBoxTree& ltree, DT_BBoxTree& rtree) const;
    void makeChildren(const DT_CBox& added, DT_BBoxTree& ltree, DT_BBoxTree& rtree) const;

    DT_CBox hull() const { return m_lbox.hull(m_rbox); }  
    
    enum FlagType { LLEAF = 0x80, RLEAF = 0x40 };

    DT_CBox              m_lbox;
    DT_CBox              m_rbox;
    DT_Index             m_lchild;
    DT_Index             m_rchild;
    unsigned char        m_flags;
};

inline void DT_BBoxNode::makeChildren(DT_BBoxTree& ltree, DT_BBoxTree& rtree) const
{
    new (&ltree) DT_BBoxTree(m_lbox, m_lchild, (m_flags & LLEAF) ? DT_BBoxTree::LEAF : DT_BBoxTree::INTERNAL);
    new (&rtree) DT_BBoxTree(m_rbox, m_rchild, (m_flags & RLEAF) ? DT_BBoxTree::LEAF : DT_BBoxTree::INTERNAL);

}

inline void DT_BBoxNode::makeChildren(const DT_CBox& added, DT_BBoxTree& ltree, DT_BBoxTree& rtree) const
{ 
    new (&ltree) DT_BBoxTree(m_lbox + added, m_lchild, (m_flags & LLEAF) ? DT_BBoxTree::LEAF : DT_BBoxTree::INTERNAL);
    new (&rtree) DT_BBoxTree(m_rbox + added, m_rchild, (m_flags & RLEAF) ? DT_BBoxTree::LEAF : DT_BBoxTree::INTERNAL);
}


template <typename Shape>
class DT_RootData {
public:
    DT_RootData(const DT_BBoxNode *nodes, 
                const Shape *leaves) 
      : m_nodes(nodes),
        m_leaves(leaves)
    {}

    const DT_BBoxNode   *m_nodes;
    const Shape         *m_leaves;
};

template <typename Shape1, typename Shape2>
class DT_ObjectData : public DT_RootData<Shape1> {
public:
    DT_ObjectData(const DT_BBoxNode *nodes, 
                  const Shape1 *leaves, 
                  const MT_Transform& xform, 
                  Shape2 plus) 
      : DT_RootData<Shape1>(nodes, leaves),
        m_xform(xform),
        m_inv_xform(xform.inverse()),   
        m_plus(plus),
        m_added(computeCBox(plus, m_inv_xform))
    {}

    const MT_Transform&  m_xform;
    MT_Transform         m_inv_xform;
    Shape2               m_plus;
    DT_CBox              m_added;
};

template <typename Shape1, typename Shape2>
class DT_Pack {
public:
    DT_Pack(const DT_ObjectData<Shape1, Shape2>& a, const DT_Convex& b)
      : m_a(a),
        m_b(b),
        m_b_cbox(b.bbox(m_a.m_inv_xform))
    {}
    
    DT_ObjectData<Shape1, Shape2>  m_a;
    const DT_Convex&               m_b;
    DT_CBox                        m_b_cbox;
};

template <typename Shape1, typename Shape2>
class DT_HybridPack : public DT_Pack<Shape1, Shape2> {
public:
    DT_HybridPack(const DT_ObjectData<Shape1, Shape2>& a, const DT_Convex& b, MT_Scalar margin)
      : DT_Pack<Shape1, Shape2>(a, b),
        m_margin(margin)
    {
        this->m_b_cbox += computeCBox(margin, this->m_a.m_inv_xform);
    }
    
    MT_Scalar m_margin;
};

template <typename Shape1, typename Shape2>
class DT_DuoPack {
public:
    DT_DuoPack(const DT_ObjectData<Shape1, Shape2>& a, const DT_ObjectData<Shape1, Shape2>& b) 
      : m_a(a),
        m_b(b)
    {
        m_b2a = a.m_inv_xform * b.m_xform;
        m_a2b = b.m_inv_xform * a.m_xform;
        m_abs_b2a = m_b2a.getBasis().absolute();
        m_abs_a2b = m_a2b.getBasis().absolute();    
    }
    
    DT_ObjectData<Shape1, Shape2>  m_a, m_b;
    MT_Transform                   m_b2a, m_a2b;
    MT_Matrix3x3                   m_abs_b2a, m_abs_a2b;
};


template <typename Shape>
inline void refit(DT_BBoxNode& node, const DT_RootData<Shape>& rd)
{
    node.m_lbox = (node.m_flags & DT_BBoxNode::LLEAF) ? 
                  computeCBox(rd.m_leaves[node.m_lchild]) : 
                  rd.m_nodes[node.m_lchild].hull(); 
    node.m_rbox = (node.m_flags & DT_BBoxNode::RLEAF) ? 
                  computeCBox(rd.m_leaves[node.m_rchild]) : 
                  rd.m_nodes[node.m_rchild].hull(); 
}


template <typename Shape>
bool ray_cast(const DT_BBoxTree& a, const DT_RootData<Shape>& rd,
              const MT_Point3& source, const MT_Point3& target, 
              MT_Scalar& lambda, MT_Vector3& normal) 
{
    if (!a.m_cbox.overlapsLineSegment(source, source.lerp(target, lambda))) 
    {
        return false;
    }

    if (a.m_type == DT_BBoxTree::LEAF) 
    { 
        return ray_cast(rd, a.m_index, source, target, lambda, normal); 
    }
    else 
    {
        DT_BBoxTree ltree, rtree;
        rd.m_nodes[a.m_index].makeChildren(ltree, rtree);
        
        bool lresult = ray_cast(ltree, rd, source, target, lambda, normal);
        bool rresult = ray_cast(rtree, rd, source, target, lambda, normal);
        return lresult || rresult;
    }
}


#ifdef STATISTICS
int num_box_tests = 0;
#endif

template <typename Shape1, typename Shape2>
inline bool intersect(const DT_CBox& a, const DT_CBox& b, const DT_DuoPack<Shape1, Shape2>& pack)
{
#ifdef STATISTICS
    ++num_box_tests;
#endif

    
    MT_Vector3 abs_pos_b2a = (pack.m_b2a(b.getCenter()) - a.getCenter()).absolute();
    MT_Vector3 abs_pos_a2b = (pack.m_a2b(a.getCenter()) - b.getCenter()).absolute();
    return  (a.getExtent()[0] + pack.m_abs_b2a[0].dot(b.getExtent()) >=  abs_pos_b2a[0]) && 
            (a.getExtent()[1] + pack.m_abs_b2a[1].dot(b.getExtent()) >=  abs_pos_b2a[1]) && 
            (a.getExtent()[2] + pack.m_abs_b2a[2].dot(b.getExtent()) >=  abs_pos_b2a[2]) && 
            (b.getExtent()[0] + pack.m_abs_a2b[0].dot(a.getExtent()) >=  abs_pos_a2b[0]) && 
            (b.getExtent()[1] + pack.m_abs_a2b[1].dot(a.getExtent()) >=  abs_pos_a2b[1]) &&
            (b.getExtent()[2] + pack.m_abs_a2b[2].dot(a.getExtent()) >=  abs_pos_a2b[2]);
}




template <typename Shape1, typename Shape2>
bool intersect(const DT_BBoxTree& a, const DT_Pack<Shape1, Shape2>& pack, MT_Vector3& v)
{ 
    if (!a.m_cbox.overlaps(pack.m_b_cbox)) 
    {
        return false;
    }

    if (a.m_type == DT_BBoxTree::LEAF) 
    {
        return intersect(pack, a.m_index, v);
    }
    else 
    {
        DT_BBoxTree a_ltree, a_rtree;
        pack.m_a.m_nodes[a.m_index].makeChildren(pack.m_a.m_added, a_ltree, a_rtree);
        return intersect(a_ltree, pack, v) || intersect(a_rtree, pack, v);
    }
}

template <typename Shape1, typename Shape2>
bool intersect(const DT_BBoxTree& a, const DT_BBoxTree& b, const DT_DuoPack<Shape1, Shape2>& pack, MT_Vector3& v)
{ 
    if (!intersect(a.m_cbox, b.m_cbox, pack)) 
    {
        return false;
    }

    if (a.m_type == DT_BBoxTree::LEAF && b.m_type == DT_BBoxTree::LEAF) 
    {
        return intersect(pack, a.m_index, b.m_index, v);
    }
    else if (a.m_type == DT_BBoxTree::LEAF || 
             (b.m_type != DT_BBoxTree::LEAF && a.m_cbox.size() < b.m_cbox.size())) 
    {
        DT_BBoxTree b_ltree, b_rtree;
        pack.m_b.m_nodes[b.m_index].makeChildren(pack.m_b.m_added, b_ltree, b_rtree);

        return intersect(a, b_ltree, pack, v) || intersect(a, b_rtree, pack, v);
    }
    else 
    {
        DT_BBoxTree a_ltree, a_rtree;
        pack.m_a.m_nodes[a.m_index].makeChildren(pack.m_a.m_added, a_ltree, a_rtree);
        return intersect(a_ltree, b, pack, v) || intersect(a_rtree, b, pack, v);
    }
}

template <typename Shape1, typename Shape2>
bool common_point(const DT_BBoxTree& a, const DT_Pack<Shape1, Shape2>& pack,  
                  MT_Vector3& v, MT_Point3& pa, MT_Point3& pb)
{ 
    if (!a.m_cbox.overlaps(pack.m_b_cbox))
    {
        return false;
    }

    if (a.m_type == DT_BBoxTree::LEAF) 
    {
        return common_point(pack, a.m_index, v, pa, pb);
    }
    else 
    {
        DT_BBoxTree a_ltree, a_rtree;
        pack.m_a.m_nodes[a.m_index].makeChildren(pack.m_a.m_added, a_ltree, a_rtree);
        return common_point(a_ltree, pack, v, pa, pb) ||
               common_point(a_rtree, pack, v, pa ,pb);
    }
}

template <typename Shape1, typename Shape2>
bool common_point(const DT_BBoxTree& a, const DT_BBoxTree& b, const DT_DuoPack<Shape1, Shape2>& pack,  
                  MT_Vector3& v, MT_Point3& pa, MT_Point3& pb)
{ 
    if (!intersect(a.m_cbox, b.m_cbox, pack))
    {
        return false;
    }

    if (a.m_type == DT_BBoxTree::LEAF && b.m_type == DT_BBoxTree::LEAF) 
    {
        return common_point(pack, a.m_index, b.m_index, v, pa, pb);
    }
    else if (a.m_type == DT_BBoxTree::LEAF || 
             (b.m_type != DT_BBoxTree::LEAF && a.m_cbox.size() < b.m_cbox.size())) 
    {
        DT_BBoxTree b_ltree, b_rtree;
        pack.m_b.m_nodes[b.m_index].makeChildren(pack.m_b.m_added, b_ltree, b_rtree);
        return common_point(a, b_ltree, pack, v, pa, pb) ||
               common_point(a, b_rtree, pack, v, pa, pb);
    }
    else 
    {
        DT_BBoxTree a_ltree, a_rtree;
        pack.m_a.m_nodes[a.m_index].makeChildren(pack.m_a.m_added, a_ltree, a_rtree);
        return common_point(a_ltree, b, pack, v, pa, pb) ||
               common_point(a_rtree, b, pack, v, pa ,pb);
    }
}


template <typename Shape1, typename Shape2>
bool penetration_depth(const DT_BBoxTree& a, const DT_HybridPack<Shape1, Shape2>& pack, 
                       MT_Vector3& v, MT_Point3& pa, MT_Point3& pb, MT_Scalar& max_pen_len) 
{ 
    if (!a.m_cbox.overlaps(pack.m_b_cbox))
    {
        return false;
    }
    
    if (a.m_type == DT_BBoxTree::LEAF) 
    {
        if (penetration_depth(pack, a.m_index, v, pa, pb))
        {
            max_pen_len = pa.distance2(pb);
            return true;
        }
        else 
        {
            return false;
        }
    }
    else 
    {
        DT_BBoxTree a_ltree, a_rtree;
        pack.m_a.m_nodes[a.m_index].makeChildren(pack.m_a.m_added, a_ltree, a_rtree);
        if (penetration_depth(a_ltree, pack, v, pa, pb, max_pen_len)) 
        {
            MT_Vector3 rv;
            MT_Point3 rpa, rpb;
            MT_Scalar rmax_pen_len;
            if (penetration_depth(a_rtree, pack, rv, rpa, rpb, rmax_pen_len) &&
                (max_pen_len < rmax_pen_len))
            {
                max_pen_len = rmax_pen_len;
                v = rv;
                pa = rpa;
                pb = rpb;
            }
            return true;
        }
        else 
        {
            return penetration_depth(a_rtree, pack, v, pa, pb, max_pen_len);
        }
    }
}

template <typename Shape1, typename Shape2>
bool penetration_depth(const DT_BBoxTree& a, const DT_BBoxTree& b, const DT_DuoPack<Shape1, Shape2>& pack, 
                       MT_Vector3& v, MT_Point3& pa, MT_Point3& pb, MT_Scalar& max_pen_len) 
{ 
    if (!intersect(a.m_cbox, b.m_cbox, pack))
    {
        return false;
    }
  
    if (a.m_type == DT_BBoxTree::LEAF && b.m_type == DT_BBoxTree::LEAF) 
    {
        if (penetration_depth(pack, a.m_index, b.m_index, v, pa, pb))
        {
            max_pen_len = pa.distance2(pb);
            return true;
        }
        else 
        {
            return false;
        }
    }
    else if (a.m_type == DT_BBoxTree::LEAF || 
             (b.m_type != DT_BBoxTree::LEAF && a.m_cbox.size() < b.m_cbox.size())) 
    {
        DT_BBoxTree b_ltree, b_rtree;
        pack.m_b.m_nodes[b.m_index].makeChildren(pack.m_b.m_added, b_ltree, b_rtree);
        if (penetration_depth(a, b_ltree, pack, v, pa, pb, max_pen_len)) 
        {
            MT_Point3 rpa, rpb;
            MT_Scalar rmax_pen_len;
            if (penetration_depth(a, b_rtree, pack, v, rpa, rpb, rmax_pen_len) &&
                (max_pen_len < rmax_pen_len))
            {
                max_pen_len = rmax_pen_len;
                pa = rpa;
                pb = rpb;
            }
            return true;
        }
        else
        {
            return penetration_depth(a, b_rtree, pack, v, pa, pb, max_pen_len);
        }
    }
    else 
    {
        DT_BBoxTree a_ltree, a_rtree;
        pack.m_a.m_nodes[a.m_index].makeChildren(pack.m_a.m_added, a_ltree, a_rtree);
        if (penetration_depth(a_ltree, b, pack, v, pa, pb, max_pen_len)) 
        {
            MT_Point3 rpa, rpb;
            MT_Scalar rmax_pen_len;
            if (penetration_depth(a_rtree, b, pack, v, rpa, rpb, rmax_pen_len) &&
                (max_pen_len < rmax_pen_len))
            {
                max_pen_len = rmax_pen_len;
                pa = rpa;
                pb = rpb;
            }
            return true;
        }
        else 
        {
            return penetration_depth(a_rtree, b, pack, v, pa, pb, max_pen_len);
        }
    }
}


// Returns a lower bound for the distance for quick rejection in closest_points
inline MT_Scalar distance2(const DT_CBox& a, const MT_Transform& a2w,
                           const DT_CBox& b, const MT_Transform& b2w)
{
    MT_Vector3 v = b2w(b.getCenter()) - a2w(a.getCenter());
    MT_Scalar dist2 = v.length2();
    if (dist2 > MT_Scalar(0.0))
    {
        MT_Vector3 w = b2w(b.support(-v * b2w.getBasis())) - a2w(a.support(v * a2w.getBasis()));
        MT_Scalar delta = v.dot(w);
        return delta > MT_Scalar(0.0) ? delta * delta / dist2 : MT_Scalar(0.0);
    }
    return MT_Scalar(0.0);
}


template <typename Shape1, typename Shape2>
MT_Scalar closest_points(const DT_BBoxTree& a, const DT_Pack<Shape1, Shape2>& pack, 
                         MT_Scalar max_dist2, MT_Point3& pa, MT_Point3& pb) 
{ 
    if (a.m_type == DT_BBoxTree::LEAF) 
    {
        return closest_points(pack, a.m_index, max_dist2, pa, pb);
    }
    else 
    {
        DT_BBoxTree a_ltree, a_rtree;
        pack.m_a.m_nodes[a.m_index].makeChildren(pack.m_a.m_added, a_ltree, a_rtree);
        MT_Scalar ldist2 = distance2(a_ltree.m_cbox, pack.m_a.m_xform, pack.m_b_cbox, pack.m_a.m_xform);
        MT_Scalar rdist2 = distance2(a_rtree.m_cbox, pack.m_a.m_xform, pack.m_b_cbox, pack.m_a.m_xform);
        if (ldist2 < rdist2) 
        {
            MT_Scalar dist2 = ldist2 < max_dist2 ? closest_points(a_ltree, pack, max_dist2, pa, pb) : MT_INFINITY;
            GEN_set_min(max_dist2, dist2);
            return rdist2 < max_dist2 ? GEN_min(dist2, closest_points(a_rtree, pack, max_dist2, pa, pb)) : dist2;
        }
        else
        {
            MT_Scalar dist2 = rdist2 < max_dist2 ? closest_points(a_rtree, pack, max_dist2, pa, pb) : MT_INFINITY;
            GEN_set_min(max_dist2, dist2);  
            return ldist2 < max_dist2 ? GEN_min(dist2, closest_points(a_ltree, pack, max_dist2, pa, pb)) : dist2;       
        }
    }
}

    
template <typename Shape1, typename Shape2>
MT_Scalar closest_points(const DT_BBoxTree& a, const DT_BBoxTree& b, const DT_DuoPack<Shape1, Shape2>& pack, 
                         MT_Scalar max_dist2, MT_Point3& pa, MT_Point3& pb) 
{   
    if (a.m_type == DT_BBoxTree::LEAF && b.m_type == DT_BBoxTree::LEAF) 
    {
        return closest_points(pack, a.m_index, b.m_index, max_dist2, pa, pb);
    }
    else if (a.m_type == DT_BBoxTree::LEAF || 
             (b.m_type != DT_BBoxTree::LEAF && a.m_cbox.size() < b.m_cbox.size())) 
    {
        DT_BBoxTree b_ltree, b_rtree;
        pack.m_b.m_nodes[b.m_index].makeChildren(pack.m_b.m_added, b_ltree, b_rtree);
        MT_Scalar ldist2 = distance2(a.m_cbox, pack.m_a.m_xform, b_ltree.m_cbox, pack.m_b.m_xform);
        MT_Scalar rdist2 = distance2(a.m_cbox, pack.m_a.m_xform, b_rtree.m_cbox, pack.m_b.m_xform);
        if (ldist2 < rdist2)
        {
            MT_Scalar dist2 = ldist2 < max_dist2 ? closest_points(a, b_ltree, pack, max_dist2, pa, pb): MT_INFINITY;;
            GEN_set_min(max_dist2, dist2);
            return rdist2 < max_dist2 ? GEN_min(dist2, closest_points(a, b_rtree, pack, max_dist2, pa, pb)) : dist2;        
        }
        else
        {
            MT_Scalar dist2 =  rdist2 < max_dist2 ? closest_points(a, b_rtree, pack, max_dist2, pa, pb) : MT_INFINITY;;
            GEN_set_min(max_dist2, dist2);
            return ldist2 < max_dist2 ? GEN_min(dist2, closest_points(a, b_ltree, pack, max_dist2, pa, pb)) : dist2;
        }
    }
    else
    {
        DT_BBoxTree a_ltree, a_rtree;
        pack.m_a.m_nodes[a.m_index].makeChildren(pack.m_a.m_added, a_ltree, a_rtree);
        MT_Scalar ldist2 = distance2(a_ltree.m_cbox, pack.m_a.m_xform, b.m_cbox, pack.m_b.m_xform);
        MT_Scalar rdist2 = distance2(a_rtree.m_cbox, pack.m_a.m_xform, b.m_cbox, pack.m_b.m_xform);
        if (ldist2 < rdist2) 
        {
            MT_Scalar dist2 = ldist2 < max_dist2 ? closest_points(a_ltree, b, pack, max_dist2, pa, pb) : MT_INFINITY;;
            GEN_set_min(max_dist2, dist2);
            return rdist2 < max_dist2 ? GEN_min(dist2,closest_points(a_rtree, b, pack, max_dist2, pa, pb)) : dist2;
        }
        else
        {
            MT_Scalar dist2 = rdist2 < max_dist2 ? closest_points(a_rtree, b, pack, max_dist2, pa, pb) : MT_INFINITY;
            GEN_set_min(max_dist2, dist2);
            return ldist2 < max_dist2 ? GEN_min(dist2, closest_points(a_ltree, b, pack, max_dist2, pa, pb)) : dist2;
        }
    }
}

#endif

