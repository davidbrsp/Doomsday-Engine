/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2008 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * p_maputil.c: Map Utility Routines
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "de_base.h"
#include "de_play.h"
#include "de_refresh.h"
#include "de_misc.h"
#include "p_bmap.h"

// MACROS ------------------------------------------------------------------

#define ORDER(x,y,a,b)  ( (x)<(y)? ((a)=(x),(b)=(y)) : ((b)=(x),(a)=(y)) )

// Linkstore is list of pointers gathered when iterating stuff.
// This is pretty much the only way to avoid *all* potential problems
// caused by callback routines behaving badly (moving or destroying
// mobjs). The idea is to get a snapshot of all the objects being
// iterated before any callbacks are called. The hardcoded limit is
// a drag, but I'd like to see you iterating 2048 mobjs/lines in one block.

#define MAXLINKED           2048
#define DO_LINKS(it, end)   for(it = linkstore; it < end; it++) \
                                if(!func(*it, data)) return false;

// TYPES -------------------------------------------------------------------

typedef struct {
    mobj_t     *mo;
    vec2_t      box[2];
} linelinker_data_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

float opentop, openbottom, openrange;
float lowfloor;

divline_t traceLOS;
boolean earlyout;
int     ptflags;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

float P_AccurateDistanceFixed(fixed_t dx, fixed_t dy)
{
    float   fx = FIX2FLT(dx), fy = FIX2FLT(dy);

    return (float) sqrt(fx * fx + fy * fy);
}

float P_AccurateDistance(float dx, float dy)
{
    return (float) sqrt(dx * dx + dy * dy);
}

/**
 * Gives an estimation of distance (not exact).
 */
float P_ApproxDistance(float dx, float dy)
{
    dx = fabs(dx);
    dy = fabs(dy);

    return dx + dy - ((dx < dy ? dx : dy) / 2);
}

/**
 * Gives an estimation of 3D distance (not exact).
 * The Z axis aspect ratio is corrected.
 */
float P_ApproxDistance3(float dx, float dy, float dz)
{
    return P_ApproxDistance(P_ApproxDistance(dx, dy), dz * 1.2f);
}

/**
 * Returns a two-component float unit vector parallel to the line.
 */
void P_LineUnitVector(line_t *line, float *unitvec)
{
    float       len = M_ApproxDistancef(line->dX, line->dY);

    if(len)
    {
        unitvec[VX] = line->dX / len;
        unitvec[VY] = line->dY / len;
    }
    else
    {
        unitvec[VX] = unitvec[VY] = 0;
    }
}

/**
 * Either end or fixpoint must be specified. The distance is measured
 * (approximately) in 3D. Start must always be specified.
 */
float P_MobjPointDistancef(mobj_t *start, mobj_t *end, float *fixpoint)
{
    if(!start)
        return 0;

    if(end)
    {
        // Start -> end.
        return M_ApproxDistancef(end->pos[VZ] - start->pos[VZ],
                                 M_ApproxDistancef(end->pos[VX] - start->pos[VX],
                                                   end->pos[VY] - start->pos[VY]));
    }

    if(fixpoint)
    {
        float       sp[3];

        sp[VX] = start->pos[VX];
        sp[VY] = start->pos[VY],
        sp[VZ] = start->pos[VZ];

        return M_ApproxDistancef(fixpoint[VZ] - sp[VZ],
                                 M_ApproxDistancef(fixpoint[VX] - sp[VX],
                                                   fixpoint[VY] - sp[VY]));
    }

    return 0;
}

/**
 * Determines on which side of dline the point is. Returns true if the
 * point is on the line or on the right side.
 */
#ifdef _MSC_VER
#  pragma optimize("g", off)
#endif
int P_FloatPointOnLineSide(fvertex_t *pnt, fdivline_t *dline)
{
    /*
       (AY-CY)(BX-AX)-(AX-CX)(BY-AY)
       s = -----------------------------
       L**2

       If s<0      C is left of AB (you can just check the numerator)
       If s>0      C is right of AB
       If s=0      C is on AB
     */
    // We'll return false if the point c is on the left side.
    return ((dline->pos[VY] - pnt->pos[VY]) * dline->dX -
            (dline->pos[VX] - pnt->pos[VX]) * dline->dY >= 0);
}

/**
 * Lines start, end and fdiv must intersect.
 */
float P_FloatInterceptVertex(fvertex_t *start, fvertex_t *end,
                             fdivline_t *fdiv, fvertex_t *inter)
{
    float   ax = start->pos[VX], ay = start->pos[VY];
    float   bx = end->pos[VX], by = end->pos[VY];
    float   cx = fdiv->pos[VX], cy = fdiv->pos[VY];
    float   dx = cx + fdiv->dX, dy = cy + fdiv->dY;

    /*
       (YA-YC)(XD-XC)-(XA-XC)(YD-YC)
       r = -----------------------------  (eqn 1)
       (XB-XA)(YD-YC)-(YB-YA)(XD-XC)
     */

    float   r =
        ((ay - cy) * (dx - cx) -
         (ax - cx) * (dy - cy)) / ((bx - ax) * (dy - cy) - (by - ay) * (dx -
                                                                        cx));
    /*
       XI=XA+r(XB-XA)
       YI=YA+r(YB-YA)
     */
    inter->pos[VX] = ax + r * (bx - ax);
    inter->pos[VY] = ay + r * (by - ay);
    return r;
}
#ifdef _MSC_VER
#  pragma optimize("", on)
#endif

/**
 * Returns 0 or 1
 */
int P_PointOnLineSide(float x, float y, line_t *line)
{
    vertex_t   *vtx1 = line->L_v1;

    return  !line->dX? x <= vtx1->V_pos[VX]? line->dY > 0 : line->dY <
        0 : !line->dY? y <= vtx1->V_pos[VY]? line->dX < 0 : line->dX >
        0 : (y - vtx1->V_pos[VY]) * line->dX >= line->dY * (x - vtx1->V_pos[VX]);
}

/**
 * Considers the line to be infinite.
 *
 * Returns side 0 or 1, -1 if box crosses the line
 */
int P_BoxOnLineSide2(float xl, float xh, float yl, float yh, line_t *ld)
{
    int         p;

    switch(ld->slopeType)
    {
    default: // shutup compiler.
    case ST_HORIZONTAL:
    {
        float ly = ld->L_v1pos[VY];
        return (yl > ly) ==
                (p = yh > ly) ? p ^ (ld->dX < 0) : -1;
    }
    case ST_VERTICAL:
    {
        float lx = ld->L_v1pos[VX];
        return (xl < lx) ==
                (p = xh < lx) ? p ^ (ld->dY < 0) : -1;
    }
    case ST_POSITIVE:
        return P_PointOnLineSide(xh, yl, ld) ==
                (p = P_PointOnLineSide(xl, yh, ld)) ? p : -1;

    case ST_NEGATIVE:
        return (P_PointOnLineSide(xl, yl, ld)) ==
                (p = P_PointOnLineSide(xh, yh, ld)) ? p : -1;
    }
}

int P_BoxOnLineSide(float *box, line_t *ld)
{
    return P_BoxOnLineSide2(box[BOXLEFT], box[BOXRIGHT],
                           box[BOXBOTTOM], box[BOXTOP], ld);
}

/**
 * Returns 0 or 1
 */
int P_PointOnDivlineSide(float fx, float fy, divline_t *line)
{
    fixed_t     x = FLT2FIX(fx);
    fixed_t     y = FLT2FIX(fy);

    return  !line->dX? x <= line->pos[VX]? line->dY > 0 : line->dY <
        0 : !line->dY? y <= line->pos[VY]? line->dX < 0 : line->dX >
        0 : (line->dY ^ line->dX ^ (x -= line->pos[VX]) ^ (y -= line->pos[VY])) <
        0? (line->dY ^ x) < 0 : FixedMul(y >> 8, line->dX >> 8) >=
        FixedMul(line->dY >> 8, x >> 8);
}

void P_MakeDivline(line_t *li, divline_t *dl)
{
    vertex_t    *vtx = li->L_v1;

    dl->pos[VX] = FLT2FIX(vtx->V_pos[VX]);
    dl->pos[VY] = FLT2FIX(vtx->V_pos[VY]);
    dl->dX = FLT2FIX(li->dX);
    dl->dY = FLT2FIX(li->dY);
}

/**
 * Returns the fractional intercept point along the first divline
 *
 * This is only called by the addmobjs and addlines traversers
 */
float P_InterceptVector(divline_t *v2, divline_t *v1)
{
    float       frac = 0;
    fixed_t     den =
        FixedMul(v1->dY >> 8, v2->dX) - FixedMul(v1->dX >> 8, v2->dY);

    if(den)
    {
        frac =
            FIX2FLT(FixedDiv((FixedMul((v1->pos[VX] - v2->pos[VX]) >> 8, v1->dY) +
                  FixedMul((v2->pos[VY] - v1->pos[VY]) >> 8, v1->dX)), den));
    }

    return frac;
}

/**
 * Sets opentop and openbottom to the window through a two sided line
 * OPTIMIZE: keep this precalculated
 */
void P_LineOpening(line_t *linedef)
{
    sector_t *front, *back;

    if(!linedef->L_backside)
    {   // Single sided line.
        openrange = 0;
        return;
    }

    front = linedef->L_frontsector;
    back = linedef->L_backsector;

    if(front->SP_ceilheight < back->SP_ceilheight)
        opentop = front->SP_ceilheight;
    else
        opentop = back->SP_ceilheight;

    if(front->SP_floorheight > back->SP_floorheight)
    {
        openbottom = front->SP_floorheight;
        lowfloor = back->SP_floorheight;
    }
    else
    {
        openbottom = back->SP_floorheight;
        lowfloor = front->SP_floorheight;
    }

    openrange = opentop - openbottom;
}

/**
 * Returns a pointer to the root linkmobj_t of the given mobj. If such
 * a block does not exist, NULL is returned. This routine is exported
 * for use in Games.
 */
mobj_t *P_GetBlockRoot(uint blockx, uint blocky)
{
    gamemap_t  *map = P_GetCurrentMap();
    uint        bmapSize[2];

    P_GetBlockmapDimensions(map->blockMap, bmapSize);

    // We must be in the block map range.
    if(blockx >= bmapSize[VX] || blocky >= bmapSize[VY])
    {
        return NULL;
    }

    return (mobj_t *) (blockrings + (blocky * bmapSize[VX] + blockx));
}

/**
 * Same as P_GetBlockRoot, but takes world coordinates as parameters.
 */
mobj_t *P_GetBlockRootXY(float x, float y)
{
    uint        blockX, blockY;

    P_PointToBlock(x, y, &blockX, &blockY);

    return P_GetBlockRoot(blockX, blockY);
}

/**
 * Only call if it is certain the mobj is linked to a sector!
 * Two links to update:
 * 1) The link to us from the previous node (sprev, always set) will
 *    be modified to point to the node following us.
 * 2) If there is a node following us, set its sprev pointer to point
 *    to the pointer that points back to it (our sprev, just modified).
 */
void P_UnlinkFromSector(mobj_t *mo)
{
    if((*mo->sPrev = mo->sNext))
        mo->sNext->sPrev = mo->sPrev;

    // Not linked any more.
    mo->sNext = NULL;
    mo->sPrev = NULL;
}

/**
 * Only call if it is certain that the mobj is linked to a block!
 */
void P_UnlinkFromBlock(mobj_t *mo)
{
    (mo->bNext->bPrev = mo->bPrev)->bNext = mo->bNext;
    // Not linked any more.
    mo->bNext = mo->bPrev = NULL;
}

/**
 * Unlinks the mobj from all the lines it's been linked to. Can be called
 * without checking that the list does indeed contain lines.
 */
void P_UnlinkFromLines(mobj_t *mo)
{
    linknode_t *tn = mobjNodes->nodes;
    nodeindex_t nix;

    // Try unlinking from lines.
    if(!mo->lineRoot)
        return; // A zero index means it's not linked.

    // Unlink from each line.
    for(nix = tn[mo->lineRoot].next; nix != mo->lineRoot;
        nix = tn[nix].next)
    {
        // Data is the linenode index that corresponds this mobj.
        NP_Unlink(lineNodes, tn[nix].data);
        // We don't need these nodes any more, mark them as unused.
        // Dismissing is a macro.
        NP_Dismiss(lineNodes, tn[nix].data);
        NP_Dismiss(mobjNodes, nix);
    }

    // The mobj no longer has a line ring.
    NP_Dismiss(mobjNodes, mo->lineRoot);
    mo->lineRoot = 0;
}

/**
 * Unlinks a mobj from everything it has been linked to.
 */
void P_MobjUnlink(mobj_t *mo)
{
    if(IS_SECTOR_LINKED(mo))
        P_UnlinkFromSector(mo);
    if(IS_BLOCK_LINKED(mo))
        P_UnlinkFromBlock(mo);
    P_UnlinkFromLines(mo);
}

/**
 * The given line might cross the mobj. If necessary, link the mobj into
 * the line's mobj link ring.
 */
boolean PIT_LinkToLines(line_t *ld, void *parm)
{
    int         pX, pY;
    linelinker_data_t *data = parm;
    vec2_t      box[2], point;
    nodeindex_t nix;

    // Setup the bounding box for the line.
    pX = (ld->L_v1pos[VX] < ld->L_v2pos[VX]);
    pY = (ld->L_v1pos[VY] < ld->L_v2pos[VY]);
    V2_Set(point, ld->L_vpos(pX^1)[VX], ld->L_vpos(pY^1)[VY]);
    V2_InitBox(box, point);
    V2_Set(point, ld->L_vpos(pX)[VX], ld->L_vpos(pY)[VY]);
    V2_AddToBox(box, point);

    if(data->box[1][VX] <= box[0][VX] ||
       data->box[0][VX] >= box[1][VX] ||
       data->box[1][VY] <= box[0][VY] ||
       data->box[0][VY] >= box[1][VY])
        // Bounding boxes do not overlap.
        return true;

    if(P_BoxOnLineSide2(data->box[0][VX], data->box[1][VX],
                        data->box[0][VY], data->box[1][VY], ld) != -1)
        // Line does not cross the mobj's bounding box.
        return true;

    // One sided lines will not be linked to because a mobj
    // can't legally cross one.
    if(!(ld->L_frontside || ld->L_backside))
        return true;

    // No redundant nodes will be creates since this routine is
    // called only once for each line.

    // Add a node to the mobj's ring.
    NP_Link(mobjNodes, nix = NP_New(mobjNodes, ld), data->mo->lineRoot);

    // Add a node to the line's ring. Also store the linenode's index
    // into the mobjring's node, so unlinking is easy.
    NP_Link(lineNodes, mobjNodes->nodes[nix].data =
            NP_New(lineNodes, data->mo), linelinks[GET_LINE_IDX(ld)]);

    return true;
}

/**
 * \pre The mobj must be currently unlinked.
 */
void P_LinkToLines(mobj_t *mo)
{
    linelinker_data_t data;
    vec2_t      point;

    // Get a new root node.
    mo->lineRoot = NP_New(mobjNodes, NP_ROOT_NODE);

    // Set up a line iterator for doing the linking.
    data.mo = mo;
    V2_Set(point, mo->pos[VX] - mo->radius, mo->pos[VY] - mo->radius);
    V2_InitBox(data.box, point);
    V2_Set(point, mo->pos[VX] + mo->radius, mo->pos[VY] + mo->radius);
    V2_AddToBox(data.box, point);

    validCount++;
    P_AllLinesBoxIteratorv(data.box, PIT_LinkToLines, &data);
}

/**
 * Links a mobj into both a block and a subsector based on it's (x,y).
 * Sets mobj->subsector properly. Calling with flags==0 only updates
 * the subsector pointer. Can be called without unlinking first.
 */
void P_MobjLink(mobj_t *mo, byte flags)
{
    sector_t *sec;
    mobj_t *root;

    // Link into the sector.
    mo->subsector = R_PointInSubsector(mo->pos[VX], mo->pos[VY]);
    sec = mo->subsector->sector;

    if(flags & DDLINK_SECTOR)
    {
        // Unlink from the current sector, if any.
        if(mo->sPrev)
            P_UnlinkFromSector(mo);

        // Link the new mobj to the head of the list.
        // Prev pointers point to the pointer that points back to us.
        // (Which practically disallows traversing the list backwards.)

        if((mo->sNext = sec->mobjList))
            mo->sNext->sPrev = &mo->sNext;

        *(mo->sPrev = &sec->mobjList) = mo;
    }

    // Link into blockmap.
    if(flags & DDLINK_BLOCKMAP)
    {
        // Unlink from the old block, if any.
        if(mo->bNext)
            P_UnlinkFromBlock(mo);

        // Link into the block we're currently in.
        root = P_GetBlockRootXY(mo->pos[VX], mo->pos[VY]);
        if(root)
        {
            // Only link if we're inside the blockmap.
            mo->bPrev = root;
            mo->bNext = root->bNext;
            mo->bNext->bPrev = root->bNext = mo;
        }
    }

    // Link into lines.
    if(!(flags & DDLINK_NOLINE))
    {
        // Unlink from any existing lines.
        P_UnlinkFromLines(mo);

        // Link to all contacted lines.
        P_LinkToLines(mo);
    }

    // If this is a player - perform addtional tests to see if they have
    // entered or exited the void.
    if(mo->dPlayer)
    {
        ddplayer_t* player = mo->dPlayer;

        player->inVoid = true;
        if(R_IsPointInSector2(player->mo->pos[VX],
                              player->mo->pos[VY],
                              player->mo->subsector->sector))
            player->inVoid = false;
    }
}

boolean P_BlockRingMobjsIterator(uint x, uint y, boolean (*func) (mobj_t *, void *),
                                  void *data)
{
    mobj_t *mobj, *root = P_GetBlockRoot(x, y);
    void   *linkstore[MAXLINKED], **end = linkstore, **it;

    if(!root)
        return true;            // Not inside the blockmap.

    // Gather all the mobjs in the block into the list.
    for(mobj = root->bNext; mobj != root; mobj = mobj->bNext)
        *end++ = mobj;

    DO_LINKS(it, end);
    return true;
}

/**
 * The callback function will be called once for each line that crosses
 * trough the object. This means all the lines will be two-sided.
 */
boolean P_MobjLinesIterator(mobj_t *mo, boolean (*func) (line_t *, void *),
                             void *data)
{
    nodeindex_t nix;
    linknode_t *tn = mobjNodes->nodes;
    void       *linkstore[MAXLINKED], **end = linkstore, **it;

    if(!mo->lineRoot)
        return true; // No lines to process.

    for(nix = tn[mo->lineRoot].next; nix != mo->lineRoot;
        nix = tn[nix].next)
        *end++ = tn[nix].ptr;

    DO_LINKS(it, end);
    return true;
}

/**
 * Increment validCount before calling this routine. The callback function
 * will be called once for each sector the mobj is touching (totally or
 * partly inside). This is not a 3D check; the mobj may actually reside
 * above or under the sector.
 */
boolean P_MobjSectorsIterator(mobj_t *mo,
                              boolean (*func) (sector_t *, void *),
                              void *data)
{
    void       *linkstore[MAXLINKED], **end = linkstore, **it;
    nodeindex_t nix;
    linknode_t *tn = mobjNodes->nodes;
    line_t     *ld;
    sector_t   *sec;

    // Always process the mobj's own sector first.
    *end++ = sec = mo->subsector->sector;
    sec->validCount = validCount;

    // Any good lines around here?
    if(mo->lineRoot)
    {
        for(nix = tn[mo->lineRoot].next; nix != mo->lineRoot;
            nix = tn[nix].next)
        {
            ld = (line_t *) tn[nix].ptr;

            // All these lines are two-sided. Try front side.
            sec = ld->L_frontsector;
            if(sec->validCount != validCount)
            {
                *end++ = sec;
                sec->validCount = validCount;
            }

            // And then the back side.
            if(ld->L_backside)
            {
                sec = ld->L_backsector;
                if(sec->validCount != validCount)
                {
                    *end++ = sec;
                    sec->validCount = validCount;
                }
            }
        }
    }
    DO_LINKS(it, end);
    return true;
}

boolean P_LineMobjsIterator(line_t *line, boolean (*func) (mobj_t *, void *),
                             void *data)
{
    void       *linkstore[MAXLINKED], **end = linkstore, **it;
    nodeindex_t root = linelinks[GET_LINE_IDX(line)], nix;
    linknode_t *ln = lineNodes->nodes;

    for(nix = ln[root].next; nix != root; nix = ln[nix].next)
        *end++ = ln[nix].ptr;

    DO_LINKS(it, end);
    return true;
}

/**
 * Increment validCount before using this. 'func' is called for each mobj
 * that is (even partly) inside the sector. This is not a 3D test, the
 * mobjs may actually be above or under the sector.
 *
 * (Lovely name; actually this is a combination of SectorMobjs and
 * a bunch of LineMobjs iterations.)
 */
boolean P_SectorTouchingMobjsIterator(sector_t *sector,
                                      boolean (*func) (mobj_t *, void *),
                                      void *data)
{
    void       *linkstore[MAXLINKED], **end = linkstore, **it;
    mobj_t     *mo;
    line_t     *li;
    nodeindex_t root, nix;
    linknode_t *ln = lineNodes->nodes;
    uint        i;

    // First process the mobjs that obviously are in the sector.
    for(mo = sector->mobjList; mo; mo = mo->sNext)
    {
        if(mo->validCount == validCount)
            continue;

        mo->validCount = validCount;
        *end++ = mo;
    }

    // Then check the sector's lines.
    for(i = 0; i < sector->lineCount; ++i)
    {
        li = sector->lines[i];

        // Iterate all mobjs on the line.
        root = linelinks[GET_LINE_IDX(li)];
        for(nix = ln[root].next; nix != root; nix = ln[nix].next)
        {
            mo = (mobj_t *) ln[nix].ptr;
            if(mo->validCount == validCount)
                continue;

            mo->validCount = validCount;
            *end++ = mo;
        }
    }

    DO_LINKS(it, end);
    return true;
}

boolean P_MobjsBoxIterator(const float box[4],
                            boolean (*func) (mobj_t *, void *),
                            void *data)
{
    vec2_t      bounds[2];

    bounds[0][VX] = box[BOXLEFT];
    bounds[0][VY] = box[BOXBOTTOM];
    bounds[1][VX] = box[BOXRIGHT];
    bounds[1][VY] = box[BOXTOP];

    return P_MobjsBoxIteratorv(bounds, func, data);
}

boolean P_MobjsBoxIteratorv(const arvec2_t box,
                             boolean (*func) (mobj_t *, void *),
                             void *data)
{
    uint        blockBox[4];

    P_BoxToBlockmapBlocks(BlockMap, blockBox, box);

    return P_BlockBoxMobjsIterator(BlockMap, blockBox, func, data);
}

boolean P_LinesBoxIterator(const float box[4],
                           boolean (*func) (line_t *, void *),
                           void *data)
{
    vec2_t      bounds[2];

    bounds[0][VX] = box[BOXLEFT];
    bounds[0][VY] = box[BOXBOTTOM];
    bounds[1][VX] = box[BOXRIGHT];
    bounds[1][VY] = box[BOXTOP];

    return P_LinesBoxIteratorv(bounds, func, data);
}

boolean P_LinesBoxIteratorv(const arvec2_t box,
                            boolean (*func) (line_t *, void *),
                            void *data)
{
    uint        blockBox[4];

    P_BoxToBlockmapBlocks(BlockMap, blockBox, box);

    return P_BlockBoxLinesIterator(BlockMap, blockBox, func, data);
}

/**
 * @return          @c false, if the iterator func returns @c false.
 */
boolean P_SubsectorsBoxIterator(const float box[4], sector_t *sector,
                               boolean (*func) (subsector_t *, void *),
                               void *parm)
{
    vec2_t      bounds[2];

    bounds[0][VX] = box[BOXLEFT];
    bounds[0][VY] = box[BOXBOTTOM];
    bounds[1][VX] = box[BOXRIGHT];
    bounds[1][VY] = box[BOXTOP];

    return P_SubsectorsBoxIteratorv(bounds, sector, func, parm);
}

/**
 * Same as the fixed-point version of this routine, but the bounding box
 * is specified using an vec2_t array (see m_vector.c).
 */
boolean P_SubsectorsBoxIteratorv(const arvec2_t box, sector_t *sector,
                                 boolean (*func) (subsector_t *, void *),
                                 void *data)
{
    static int  localValidCount = 0;
    uint        blockBox[4];

    // This is only used here.
    localValidCount++;

    // Blockcoords to check.
    P_BoxToBlockmapBlocks(SSecBlockMap, blockBox, box);

    return P_BlockBoxSubsectorsIterator(SSecBlockMap, blockBox, sector,
                                        box, localValidCount, func, data);
}

boolean P_PolyobjsBoxIterator(const float box[4],
                              boolean (*func) (polyobj_t *, void *),
                              void *data)
{
    vec2_t      bounds[2];

    bounds[0][VX] = box[BOXLEFT];
    bounds[0][VY] = box[BOXBOTTOM];
    bounds[1][VX] = box[BOXRIGHT];
    bounds[1][VY] = box[BOXTOP];

    return P_PolyobjsBoxIteratorv(bounds, func, data);
}

/**
 * The validCount flags are used to avoid checking polys that are marked in
 * multiple mapblocks, so increment validCount before the first call, then
 * make one or more calls to it.
 */
boolean P_PolyobjsBoxIteratorv(const arvec2_t box,
                               boolean (*func) (polyobj_t *, void *),
                               void *data)
{
    uint        blockBox[4];

    // Blockcoords to check.
    P_BoxToBlockmapBlocks(BlockMap, blockBox, box);

    return P_BlockBoxPolyobjsIterator(BlockMap, blockBox, func, data);
}

boolean P_PolyobjLinesBoxIterator(const float box[4],
                                  boolean (*func) (line_t *, void *),
                                  void *data)
{
    vec2_t      bounds[2];

    bounds[0][VX] = box[BOXLEFT];
    bounds[0][VY] = box[BOXBOTTOM];
    bounds[1][VX] = box[BOXRIGHT];
    bounds[1][VY] = box[BOXTOP];

    return P_PolyobjLinesBoxIteratorv(bounds, func, data);
}

boolean P_PolyobjLinesBoxIteratorv(const arvec2_t box,
                                   boolean (*func) (line_t *, void *),
                                   void *data)
{
    uint        blockBox[4];

    P_BoxToBlockmapBlocks(BlockMap, blockBox, box);

    return P_BlockBoxPolyobjLinesIterator(BlockMap, blockBox, func, data);
}


/**
 * The validCount flags are used to avoid checking lines that are marked
 * in multiple mapblocks, so increment validCount before the first call
 * to P_BlockmapLinesIterator, then make one or more calls to it.
 */
boolean P_AllLinesBoxIterator(const float box[4],
                              boolean (*func) (line_t *, void *),
                              void *data)
{
    vec2_t  bounds[2];

    bounds[0][VX] = box[BOXLEFT];
    bounds[0][VY] = box[BOXBOTTOM];
    bounds[1][VX] = box[BOXRIGHT];
    bounds[1][VY] = box[BOXTOP];

    return P_AllLinesBoxIteratorv(bounds, func, data);
}

/**
 * The validCount flags are used to avoid checking lines that are marked
 * in multiple mapblocks, so increment validCount before the first call
 * to P_BlockmapLinesIterator, then make one or more calls to it.
 */
boolean P_AllLinesBoxIteratorv(const arvec2_t box,
                               boolean (*func) (line_t *, void *),
                               void *data)
{
    if(numpolyobjs > 0)
    {
        if(!P_PolyobjLinesBoxIteratorv(box, func, data))
            return false;
    }

    return P_LinesBoxIteratorv(box, func, data);
}

/**
 * Looks for lines in the given block that intercept the given trace to add
 * to the intercepts list
 * A line is crossed if its endpoints are on opposite sides of the trace.
 *
 * @return              @c true if earlyout and a solid line hit.
 */
boolean PIT_AddLineIntercepts(line_t *ld, void *data)
{
    int         s[2];
    float       frac;
    divline_t   dl;

    // Avoid precision problems with two routines.
    if(traceLOS.dX > FRACUNIT * 16 || traceLOS.dY > FRACUNIT * 16 ||
       traceLOS.dX < -FRACUNIT * 16 || traceLOS.dY < -FRACUNIT * 16)
    {
        s[0] = P_PointOnDivlineSide(ld->L_v1pos[VX],
                                    ld->L_v1pos[VY], &traceLOS);
        s[1] = P_PointOnDivlineSide(ld->L_v2pos[VX],
                                    ld->L_v2pos[VY], &traceLOS);
    }
    else
    {
        s[0] = P_PointOnLineSide(FIX2FLT(traceLOS.pos[VX]),
                                 FIX2FLT(traceLOS.pos[VY]), ld);
        s[1] = P_PointOnLineSide(FIX2FLT(traceLOS.pos[VX] + traceLOS.dX),
                                 FIX2FLT(traceLOS.pos[VY] + traceLOS.dY), ld);
    }

    if(s[0] == s[1])
        return true; // Line isn't crossed.

    //
    // Hit the line.
    //
    P_MakeDivline(ld, &dl);
    frac = P_InterceptVector(&traceLOS, &dl);
    if(frac < 0)
        return true; // Behind source.

    // Try to early out the check.
    if(earlyout && frac < 1 && !ld->L_backside)
        return false; // Stop iteration.

    P_AddIntercept(frac, true, ld);

    return true; // Continue iteration.
}

boolean PIT_AddMobjIntercepts(mobj_t *mo, void *data)
{
    float       x1, y1, x2, y2;
    int         s[2];
    boolean     tracepositive;
    divline_t   dl;
    float       frac;

    if(mo->dPlayer && (mo->dPlayer->flags & DDPF_CAMERA))
        return true; // $democam: ssshh, keep going, we're not here...

    tracepositive = (traceLOS.dX ^ traceLOS.dY) > 0;

    // Check a corner to corner crossection for hit.
    if(tracepositive)
    {
        x1 = mo->pos[VX] - mo->radius;
        y1 = mo->pos[VY] + mo->radius;

        x2 = mo->pos[VX] + mo->radius;
        y2 = mo->pos[VY] - mo->radius;
    }
    else
    {
        x1 = mo->pos[VX] - mo->radius;
        y1 = mo->pos[VY] - mo->radius;

        x2 = mo->pos[VX] + mo->radius;
        y2 = mo->pos[VY] + mo->radius;
    }

    s[0] = P_PointOnDivlineSide(x1, y1, &traceLOS);
    s[1] = P_PointOnDivlineSide(x2, y2, &traceLOS);
    if(s[0] == s[1])
        return true; // Line isn't crossed.

    dl.pos[VX] = FLT2FIX(x1);
    dl.pos[VY] = FLT2FIX(y1);
    dl.dX = FLT2FIX(x2 - x1);
    dl.dY = FLT2FIX(y2 - y1);

    frac = P_InterceptVector(&traceLOS, &dl);
    if(frac < 0)
        return true; // Behind source.

    P_AddIntercept(frac, false, mo);

    return true; // Keep going.
}

/**
 * Traces a line from x1,y1 to x2,y2, calling the traverser function for each
 * Returns true if the traverser function returns true for all lines
 */
boolean P_PathTraverse(float x1, float y1, float x2, float y2,
                       int flags, boolean (*trav) (intercept_t *))
{
    float       origin[2], dest[2];
    uint        originBlock[2], destBlock[2];
    gamemap_t  *map = P_GetCurrentMap();
    vec2_t      bmapOrigin;

    P_GetBlockmapBounds(map->blockMap, bmapOrigin, NULL);

    earlyout = flags & PT_EARLYOUT;

    validCount++;
    P_ClearIntercepts();

    origin[VX] = x1;
    origin[VY] = y1;
    dest[VX] = x2;
    dest[VY] = y2;

    if((FLT2FIX(origin[VX] - bmapOrigin[VX]) & (MAPBLOCKSIZE - 1)) == 0)
        origin[VX] += 1;         // don't side exactly on a line
    if((FLT2FIX(origin[VY] - bmapOrigin[VY]) & (MAPBLOCKSIZE - 1)) == 0)
        origin[VY] += 1;         // don't side exactly on a line

    traceLOS.pos[VX] = FLT2FIX(origin[VX]);
    traceLOS.pos[VY] = FLT2FIX(origin[VY]);
    traceLOS.dX = FLT2FIX(dest[VX] - origin[VX]);
    traceLOS.dY = FLT2FIX(dest[VY] - origin[VY]);

    // \todo Clip path so that both start and end are within the blockmap.
    // If the path is completely outside the blockmap, we can be sure that
    // there is no way it can intercept with something.

    // Points should never be out of bounds but check anyway.
    if(!(P_ToBlockmapBlockIdx(map->blockMap, originBlock, origin) &&
         P_ToBlockmapBlockIdx(map->blockMap, destBlock, dest)))
    {
        return false;
    }

    origin[VX] -= bmapOrigin[VX];
    origin[VY] -= bmapOrigin[VY];
    dest[VX] -= bmapOrigin[VX];
    dest[VY] -= bmapOrigin[VY];

    if(!P_BlockPathTraverse(BlockMap, originBlock, destBlock, origin, dest,
                            flags, trav))
        return false; // Early out.

    // Go through the sorted list.
    return P_TraverseIntercepts(trav, 1.0f);
}
