/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// r_misc.c

#include "quakedef.h"

float sintablef[17] = {
    0.000000f, 0.382683f, 0.707107f,
    0.923879f, 1.000000f, 0.923879f,
    0.707107f, 0.382683f, 0.000000f,
    -0.382683f, -0.707107f, -0.923879f,
    -1.000000f, -0.923879f, -0.707107f,
    -0.382683f, 0.000000f};

float costablef[17] = {
    1.000000f, 0.923879f, 0.707107f,
    0.382683f, 0.000000f, -0.382683f,
    -0.707107f, -0.923879f, -1.000000f,
    -0.923879f, -0.707107f, -0.382683f,
    0.000000f, 0.382683f, 0.707107f,
    0.923879f, 1.000000f};

void R_InitParticles(void);
void R_ClearParticles(void);
void R_InitLights(void);
void GL_BuildLightmaps(void);
qboolean VID_Is8bit(void);

cvar_t mipbias = {"mipbias", "7"};

/* Used for generating the lightmap multiple ways for debugging */
void VID_GenerateLighting(qboolean alpha) {
#ifdef _arch_dreamcast
  static unsigned int *lightingPalette = NULL;
  if(!lightingPalette)
    lightingPalette = (unsigned int *)Z_Malloc(256 * sizeof(uint32_t));

  for (int i = 0; i < 16; i++) {
    for (int j = 0; j < 16; j++) {
      if (alpha) {
        lightingPalette[i * 16 + j] = (i * 16 + j) << 24 | 0x0 << 16 | 0x0 << 8 | 0x0;
      } else {
        lightingPalette[i * 16 + j] = 0XFF << 24 | (i * 16 + j) << 16 | (i * 16 + j) << 8 | (i * 16 + j);
      }
    }
  }
  glColorTableEXT(GL_SHARED_TEXTURE_PALETTE_1_KOS, GL_RGBA8, 256, GL_RGBA, GL_UNSIGNED_BYTE, (void *)lightingPalette);
#else
  (void)alpha;
#endif
}

void VID_GenerateLighting_f(void) {
  if (Cmd_Argc() < 1)
    return;

  VID_GenerateLighting((byte)atoi(Cmd_Argv(1)));
}

void VID_ChangeLightmapFilter_f(void) {
  extern int lightmap_filter;

  if (lightmap_filter != GL_NEAREST) {
    lightmap_filter = GL_NEAREST;
  } else {
    lightmap_filter = GL_LINEAR;
  }
}

void VID_Gamma_f(void) {
#if _arch_dreamcast
  extern unsigned char *newPalette;
  static float vid_gamma = 1.0;
  float f, inf;
  unsigned char palette[768];
  int i;

  if (Cmd_Argc() == 2) {
    vid_gamma = atoff(Cmd_Argv(1));
    for (i = 0; i < 768; i++) {
      f = powf((host_basepal[i] + 1) * (1.0f / 256.0f), vid_gamma);
      inf = f * 255 + 0.5f;
      if (inf < 0)
        inf = 0;
      if (inf > 255)
        inf = 255;
      palette[i] = inf;
    }

    VID_SetPalette(palette);
    glColorTableEXT(GL_SHARED_TEXTURE_PALETTE_EXT, GL_RGBA8, 256, GL_RGBA, GL_UNSIGNED_BYTE, (void *)newPalette);
    //Z_Free(newPalette);
    Cvar_SetValue("gamma", vid_gamma);
  }
#endif
}

/*
==================
glScaleF
==================
*/
void glScaleF(float x, float y, float z) {
  glScalef(x, y, z);
  extern void M_PrintWhite(int cx, int cy, char *str);
  M_PrintWhite(LEFT_MARGIN, 240 - BOTTOM_MARGIN, "\x6e\x6f\x74\x20\x66\x6f\x72\x20\x64\x69\x73\x74\x72\x69\x62\x75\x74\x69\x6f\x6e\x20\x74\x6f");
  M_PrintWhite(LEFT_MARGIN, 240 - (BOTTOM_MARGIN / 2), "\x22\x44\x72\x65\x61\x6d\x63\x61\x73\x74\x20\x4f\x6e\x6c\x69\x6e\x65\x22\x20\x6f\x72\x20\x22\x53\x65\x67\x61\x4e\x65\x74\x22");
}

/*
==================
R_InitTextures
==================
*/
void R_InitTextures(void) {
  int x, y, m;
  byte *dest;

  // create a simple checkerboard texture for the default
  r_notexture_mip = Hunk_AllocName(sizeof(texture_t) + 16 * 16 + 8 * 8 + 4 * 4 + 2 * 2, "notexture");

  r_notexture_mip->width = r_notexture_mip->height = 16;
  r_notexture_mip->offsets[0] = sizeof(texture_t);
  r_notexture_mip->offsets[1] = r_notexture_mip->offsets[0] + 16 * 16;
  r_notexture_mip->offsets[2] = r_notexture_mip->offsets[1] + 8 * 8;
  r_notexture_mip->offsets[3] = r_notexture_mip->offsets[2] + 4 * 4;

  for (m = 0; m < 4; m++) {
    dest = (byte *)r_notexture_mip + r_notexture_mip->offsets[m];
    for (y = 0; y < (16 >> m); y++)
      for (x = 0; x < (16 >> m); x++) {
        if ((y < (8 >> m)) ^ (x < (8 >> m)))
          *dest++ = 0;
        else
          *dest++ = 0xff;
      }
  }
}

byte __attribute__((aligned(32))) dottexture[8][8] =
    {
        {0, 1, 1, 0, 0, 0, 0, 0},
        {1, 1, 1, 1, 0, 0, 0, 0},
        {1, 1, 1, 1, 0, 0, 0, 0},
        {0, 1, 1, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0},
};
void R_InitParticleTexture(void) {
  int x, y;
  byte __attribute__((aligned(32))) data[8][8][4];

  //
  // particle texture
  //
  glGenTextures(1, (GLuint*)&particletexture);
  GL_Bind(particletexture);

  for (x = 0; x < 8; x++) {
    for (y = 0; y < 8; y++) {
      data[y][x][0] = 255;
      data[y][x][1] = 255;
      data[y][x][2] = 255;
      data[y][x][3] = dottexture[x][y] * 255;
    }
  }
  glTexImage2D(GL_TEXTURE_2D, 0, gl_alpha_format, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

  glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

/*
===============
R_Envmap_f

Grab six views for environment mapping tests
===============
*/
void R_Envmap_f(void) {
}

/*
===============
R_Init
===============
*/
void R_Init(void) {
  extern cvar_t gl_finish;

#ifndef _arch_dreamcast
  Cmd_AddCommand("timerefresh", R_TimeRefresh_f);
  Cmd_AddCommand("envmap", R_Envmap_f);
  Cmd_AddCommand("pointfile", R_ReadPointFile_f);
#endif

  Cvar_RegisterVariable(&r_norefresh);
  Cvar_RegisterVariable(&r_lightmap);
  Cvar_RegisterVariable(&r_fullbright);
  Cvar_RegisterVariable(&r_drawentities);
  Cvar_RegisterVariable(&r_drawviewmodel);
  Cvar_RegisterVariable(&r_shadows);
  Cvar_RegisterVariable(&r_mirroralpha);
  Cvar_RegisterVariable(&r_wateralpha);
  Cvar_RegisterVariable(&r_dynamic);
  Cvar_RegisterVariable(&r_novis);
  Cvar_RegisterVariable(&r_speeds);
  Cvar_RegisterVariable(&r_interpolate_anim);  // fenix@io.com: register new cvars for model interpolation
  Cvar_RegisterVariable(&r_interpolate_pos);

  Cvar_RegisterVariable(&gl_finish);
  Cvar_RegisterVariable(&gl_clear);

  Cvar_RegisterVariable(&mipbias);

  Cvar_RegisterVariable(&gl_cull);
  Cvar_RegisterVariable(&gl_smoothmodels);
  Cvar_RegisterVariable(&gl_affinemodels);
  Cvar_RegisterVariable(&gl_polyblend);
  Cvar_RegisterVariable(&gl_flashblend);
  Cvar_RegisterVariable(&gl_playermip);
  Cvar_RegisterVariable(&gl_nocolors);

  Cvar_RegisterVariable(&gl_keeptjunctions);
  Cvar_RegisterVariable(&gl_reporttjunctions);

  Cvar_RegisterVariable(&gl_doubleeyes);

  extern cvar_t r_sky;
  Cvar_RegisterVariable(&r_sky);

  R_InitParticles();
  R_InitParticleTexture();
  R_InitLights();

#ifdef GLTEST
  Test_Init();
#endif

  glGenTextures(16, (GLuint*)playertextures);
}

/*
===============
R_TranslatePlayerSkin

Translates a skin texture by the per-player color lookup
===============
*/
static byte __attribute__((aligned(32))) translated[320 * 200];
void R_TranslatePlayerSkin(int playernum) {
  int top, bottom;
  byte translate[256];

  unsigned int i, s;
  model_t *model;
  aliashdr_t *paliashdr;
  byte *original;

  GL_DisableMultitexture();

  top = cl.scores[playernum].colors & 0xf0;
  bottom = (cl.scores[playernum].colors & 15) << 4;

  for (i = 0; i < 256; i++)
    translate[i] = i;

  for (i = 0; i < 16; i++) {
    if (top < 128)  // the artists made some backwards ranges.  sigh.
      translate[TOP_RANGE + i] = top + i;
    else
      translate[TOP_RANGE + i] = top + 15 - i;

    if (bottom < 128)
      translate[BOTTOM_RANGE + i] = bottom + i;
    else
      translate[BOTTOM_RANGE + i] = bottom + 15 - i;
  }

  //
  // locate the original skin pixels
  //
  currententity = &cl_entities[1 + playernum];
  model = currententity->model;
  if (!model)
    return;  // player doesn't have a model yet
  if (model->type != mod_alias)
    return;  // only translate skins on alias models

  paliashdr = (aliashdr_t *)Mod_Extradata(model);
  s = paliashdr->skinwidth * paliashdr->skinheight;
  if (currententity->skinnum < 0 || currententity->skinnum >= paliashdr->numskins) {
    Con_Printf("(%d): Invalid player skin #%d of %d\n", playernum, currententity->skinnum, paliashdr->numskins);
    original = (byte *)paliashdr + paliashdr->texels[0];
  } else
    original = (byte *)paliashdr + paliashdr->texels[currententity->skinnum];
  if (s & 3)
    Sys_Error("R_TranslateSkin: s&3");

  // because this happens during gameplay, do it fast
  // instead of sending it through gl_upload 8
  GL_Bind(playertextures[playernum]);

  if (s > 320 * 200) {
    return;
  }

  for (i = 0; i < s; i += 4) {
    translated[i] = translate[original[i]];
    translated[i + 1] = translate[original[i + 1]];
    translated[i + 2] = translate[original[i + 2]];
    translated[i + 3] = translate[original[i + 3]];
  }

  // don't mipmap these, because it takes too long
  GL_Upload8(translated, paliashdr->skinwidth, paliashdr->skinheight, TEX_NONE);
}

/*
===============
R_NewMap
===============
*/
void R_NewMap(void) {
  int i;

  for (i = 0; i < 256; i++)
    d_lightstylevalue[i] = 264;  // normal light value

  memset(&r_worldentity, 0, sizeof(r_worldentity));
  r_worldentity.model = cl.worldmodel;

  // clear out efrags in case the level hasn't been reloaded
  // FIXME: is this one short?
  for (i = 0; i < cl.worldmodel->numleafs; i++)
    cl.worldmodel->leafs[i].efrags = NULL;

  r_viewleaf = NULL;
  R_ClearParticles();

  GL_BuildLightmaps();

  // identify sky texture
  skytexturenum = -1;
  mirrortexturenum = -1;
  for (i = 0; i < cl.worldmodel->numtextures; i++) {
    if (!cl.worldmodel->textures[i])
      continue;
    if (!strncmp(cl.worldmodel->textures[i]->name, "sky", 3))
      skytexturenum = i;
    if (!strncmp(cl.worldmodel->textures[i]->name, "window02_1", 10))
      mirrortexturenum = i;
    cl.worldmodel->textures[i]->texturechain = NULL;
  }
#ifdef QUAKE2
  R_LoadSkys();
#endif
}

/*
====================
R_TimeRefresh_f

For program optimization
====================
*/
void R_TimeRefresh_f(void) {
#if 0
  int i;
  float start, stop, time;

  glDrawBuffer(GL_FRONT);
  glFinish();

  start = Sys_FloatTime();
  for (i = 0; i < 128; i++) {
    r_refdef.viewangles[1] = i / 128.0 * 360.0;
    R_RenderView();
  }

  glFinish();
  stop = Sys_FloatTime();
  time = stop - start;
  Con_Printf("%f seconds (%f fps)\n", time, 128 / time);

  glDrawBuffer(GL_BACK);
  GL_EndRendering();
#endif
}

void D_FlushCaches(void) {
}
