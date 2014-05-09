/** @file superblockmap.cpp  BSP Builder Super Blockmap.
 *
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
 * @authors Copyright © 2000-2007 Andrew Apted <ajapted@gmail.com>
 * @authors Copyright © 1998-2000 Colin Reed <cph@moria.org.uk>
 * @authors Copyright © 1998-2000 Lee Killough <killough@rsn.hp.com>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "world/bsp/superblockmap.h"
#include <de/vector1.h>
#include <de/Log>

using namespace de;
using namespace de::bsp;

DENG2_PIMPL_NOREF(SuperBlockmapNodeData)
{
    AABox bounds;      ///< Bounds of the coordinate subspace at the node.

    Segments segments; ///< Line segments contained by the node (not owned).
    int mapNum;        ///< Running total of map-line segments at/under this node.
    int partNum;       ///< Running total of partition-line segments at/under this node.

    Instance(AABox const &bounds)
        : bounds (bounds)
        , mapNum (0)
        , partNum(0)
    {}

    inline void link(LineSegmentSide &seg) {
        segments.prepend(&seg);
    }

    inline void addRef(LineSegmentSide const &seg) {
        if(seg.hasMapSide()) mapNum++;
        else                 partNum++;
    }

    inline void decRef(LineSegmentSide const &seg) {
        if(seg.hasMapSide()) mapNum--;
        else                 partNum--;
    }
};

SuperBlockmapNodeData::SuperBlockmapNodeData(AABox const &bounds)
    : _node(0)
    , d(new Instance(bounds))
{}

AABox const &SuperBlockmapNodeData::bounds() const
{
    return d->bounds;
}

/**
 * Performs k-d tree subdivision of the 2D coordinate space, splitting the node
 * tree as necessary, however new nodes are created only when they need to be
 * populated (i.e., a split does not generate two nodes at the same time).
 */
SuperBlockmapNode &SuperBlockmapNodeData::push(LineSegmentSide &seg)
{
    // Traverse the node tree beginning at "this" node.
    SuperBlockmapNode *sb = _node;
    forever
    {
        SuperBlockmapNodeData &ndata = *sb->userData();
        AABox const &bounds = ndata.bounds();

        // The segment "touches" this node; increment the ref counters.
        ndata.d->addRef(seg);

        // Determine whether further subdivision is necessary/possible.
        Vector2i dimensions(Vector2i(bounds.max) - Vector2i(bounds.min));
        if(dimensions.x <= 256 && dimensions.y <= 256)
        {
            // Thats as small as we go; link it in and return.
            ndata.d->link(seg);
            break;
        }

        // Determine whether the node should be split and on which axis.
        int const splitAxis    = (dimensions.x < dimensions.y); // x=0, y=1
        int const midOnAxis    = (bounds.min[splitAxis] + bounds.max[splitAxis]) / 2;
        SuperBlockmapNode::ChildId fromSide = SuperBlockmapNode::ChildId(seg.from().origin()[splitAxis] >= midOnAxis);
        SuperBlockmapNode::ChildId toSide   = SuperBlockmapNode::ChildId(seg.to  ().origin()[splitAxis] >= midOnAxis);

        // Does the segment lie entirely within one half of this node?
        if(fromSide != toSide)
        {
            // No, the segment crosses @var midOnAxis; link it in and return.
            ndata.d->link(seg);
            break;
        }

        // Do we need to create the child node?
        if(!sb->hasChild(fromSide))
        {
            bool const toLeft = (fromSide == SuperBlockmapNode::Left);

            AABox childBounds;
            if(splitAxis)
            {
                // Vertical split.
                int division = bounds.minY + 0.5 + (bounds.maxY - bounds.minY) / 2;

                childBounds.minX = bounds.minX;
                childBounds.minY = (toLeft? division : bounds.minY);

                childBounds.maxX = bounds.maxX;
                childBounds.maxY = (toLeft? bounds.maxY : division);
            }
            else
            {
                // Horizontal split.
                int division = bounds.minX + 0.5 + (bounds.maxX - bounds.minX) / 2;

                childBounds.minX = (toLeft? division : bounds.minX);
                childBounds.minY = bounds.minY;

                childBounds.maxX = (toLeft? bounds.maxX : division);
                childBounds.maxY = bounds.maxY;
            }

            // Add a new child node and link it to its parent.
            SuperBlockmapNodeData *childData = new SuperBlockmapNodeData(childBounds);
            childData->_node = &sb->setChild(fromSide, new SuperBlockmapNode(childData, sb));
        }

        // Descend to the child node.
        sb = sb->childPtr(fromSide);
    }

    return *sb;
}

LineSegmentSide *SuperBlockmapNodeData::pop()
{
    if(!d->segments.isEmpty())
    {
        LineSegmentSide *seg = d->segments.takeFirst();
        d->decRef(*seg);
        return seg;
    }
    return 0;
}

int SuperBlockmapNodeData::segmentCount(bool addMap, bool addPart) const
{
    int total = 0;
    if(addMap)  total += d->mapNum;
    if(addPart) total += d->partNum;
    return total;
}

SuperBlockmapNodeData::Segments const &SuperBlockmapNodeData::segments() const
{
    return d->segments;
}
