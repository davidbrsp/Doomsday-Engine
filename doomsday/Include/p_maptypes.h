/* Generated by Scripts/makedmt.py */

#ifndef __DOOMSDAY_PLAY_MAP_DATA_TYPES_H__
#define __DOOMSDAY_PLAY_MAP_DATA_TYPES_H__

#include "p_data.h"

typedef struct vertex_s {
    runtime_mapdata_header_t header;
    fixed_t             x;
    fixed_t             y;
} vertex_t;

typedef struct seg_s {
    runtime_mapdata_header_t header;
    struct vertex_s*    v1;            // Start of the segment.
    struct vertex_s*    v2;            // End of the segment.
    float               length;        // Accurate length of the segment (v1 -> v2).
    fixed_t             offset;
    struct side_s*      sidedef;
    struct line_s*      linedef;
    struct sector_s*    frontsector;
    struct sector_s*    backsector;
    angle_t             angle;
    byte                flags;
} seg_t;

typedef struct subsector_s {
    runtime_mapdata_header_t header;
    struct sector_s*    sector;
    int                 linecount;
    int                 firstline;
    struct polyobj_s*   poly;          // NULL, if there is no polyobj.
    byte                flags;
    char                numverts;
    fvertex_t*          verts;         // A sorted list of edge vertices.
    fvertex_t           bbox[2];       // Min and max points.
    fvertex_t           midpoint;      // Center of vertices.
} subsector_t;

typedef struct plane_s {
    runtime_mapdata_header_t header;
    fixed_t             height;        // Current height.
    float               normal[3];     // Plane normal.
    short               pic;           // Texture.
    float               offx;          // Texture x offset.
    float               offy;          // Texture y offset.
    byte                rgb[3];        // Surface color tint.
    float               glow;          // Glow amount.
    byte                glowrgb[3];    // Glow color.
    int                 target;        // Target height.
    int                 speed;         // Move speed.
    int                 texmove[2];    // Texture movement X and Y.
    degenmobj_t         soundorg;      // Sound origin for plane.
    struct sector_s*    sector;        // Owner of the plane (temp)
} plane_t;

// Helper macros for accessing sector floor/ceiling plane data elements.
#define SP_ceilheight           planes[PLN_CEILING].height
#define SP_ceilnormal           planes[PLN_CEILING].normal
#define SP_ceilpic              planes[PLN_CEILING].pic
#define SP_ceiloffx             planes[PLN_CEILING].offx
#define SP_ceiloffy             planes[PLN_CEILING].offy
#define SP_ceilrgb              planes[PLN_CEILING].rgb
#define SP_ceilglow             planes[PLN_CEILING].glow
#define SP_ceilglowrgb          planes[PLN_CEILING].glowrgb
#define SP_ceiltarget           planes[PLN_CEILING].target
#define SP_ceilspeed            planes[PLN_CEILING].speed
#define SP_ceiltexmove          planes[PLN_CEILING].texmove
#define SP_ceilsoundorg         planes[PLN_CEILING].soundorg

#define SP_floorheight          planes[PLN_FLOOR].height
#define SP_floornormal          planes[PLN_FLOOR].normal
#define SP_floorpic             planes[PLN_FLOOR].pic
#define SP_flooroffx            planes[PLN_FLOOR].offx
#define SP_flooroffy            planes[PLN_FLOOR].offy
#define SP_floorrgb             planes[PLN_FLOOR].rgb
#define SP_floorglow            planes[PLN_FLOOR].glow
#define SP_floorglowrgb         planes[PLN_FLOOR].glowrgb
#define SP_floortarget          planes[PLN_FLOOR].target
#define SP_floorspeed           planes[PLN_FLOOR].speed
#define SP_floortexmove         planes[PLN_FLOOR].texmove
#define SP_floorsoundorg        planes[PLN_FLOOR].soundorg

typedef struct sector_s {
    runtime_mapdata_header_t header;
    short               lightlevel;
    byte                rgb[3];
    int                 validcount;    // if == validcount, already checked.
    struct mobj_s*      thinglist;     // List of mobjs in the sector.
    int                 linecount;
    struct line_s**     Lines;         // [linecount] size.
    int                 subscount;
    struct subsector_s** subsectors;   // [subscount] size.
    int                 skyfix;
    degenmobj_t         soundorg;
    float               reverb[NUM_REVERB_DATA];
    int                 blockbox[4];   // Mapblock bounding box.
    plane_t             planes[2];
} sector_t;

typedef struct side_s {
    runtime_mapdata_header_t header;
    fixed_t             textureoffset; // Add this to the calculated texture col.
    fixed_t             rowoffset;     // Add this to the calculated texture top.
    short               toptexture;
    short               bottomtexture;
    short               midtexture;
    byte                toprgb[3];
    byte                bottomrgb[3];
    byte                midrgba[4];
    blendmode_t         blendmode;
    struct sector_s*    sector;
    short               flags;
} side_t;

typedef struct line_s {
    runtime_mapdata_header_t header;
    struct vertex_s*    v1;
    struct vertex_s*    v2;
    short               flags;
    struct sector_s*    frontsector;
    struct sector_s*    backsector;
    fixed_t             dx;
    fixed_t             dy;
    slopetype_t         slopetype;
    int                 validcount;
    int                 sidenum[2];
    fixed_t             bbox[4];
} line_t;

typedef struct polyobj_s {
    runtime_mapdata_header_t header;
    int                 numsegs;
    struct seg_s**      segs;
    int                 validcount;
    degenmobj_t         startSpot;
    angle_t             angle;
    int                 tag;           // reference tag assigned in HereticEd
    ddvertex_t*         originalPts;   // used as the base for the rotations
    ddvertex_t*         prevPts;       // use to restore the old point values
    fixed_t             bbox[4];
    vertex_t            dest;
    int                 speed;         // Destination XY and speed.
    angle_t             destAngle;     // Destination angle.
    angle_t             angleSpeed;    // Rotation speed.
    boolean             crush;         // should the polyobj attempt to crush mobjs?
    int                 seqType;
    fixed_t             size;          // polyobj size (area of POLY_AREAUNIT == size of FRACUNIT)
    void*               specialdata;   // pointer a thinker, if the poly is moving
} polyobj_t;

typedef struct node_s {
    runtime_mapdata_header_t header;
    fixed_t             x;             // Partition line.
    fixed_t             y;             // Partition line.
    fixed_t             dx;            // Partition line.
    fixed_t             dy;            // Partition line.
    fixed_t             bbox[2][4];    // Bounding box for each child.
    int                 children[2];   // If NF_SUBSECTOR it's a subsector.
} node_t;

#endif
