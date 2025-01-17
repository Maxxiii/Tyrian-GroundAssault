/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*
*	This product contains software technology licensed from Id
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/

#include "platform.h"
#include <string.h> // strcpy
#include <stdlib.h> // atoi
#include <ctype.h>  // isspace
#include "vector.h"
#include "util_vector.h"
#include "const.h"
#include "usercmd.h"
#include "protocol.h"
#include "pm_defs.h"
#include "pm_shared.h"
#include "pm_movevars.h"
#include "pm_debug.h"
#include "pm_materials.h"// XDM: CBTEXTURENAMEMAX CTEXTURESMAX CHAR_TEX_*
#include "com_model.h"// XDM: modtype_t dclipnode_t mplane_t hull_t
#include "event_flags.h"
#include "event_args.h"
#include "cvardef.h"

#ifdef CLIENT_DLL
// Spectator Mode
int				iJumpSpectator;
float			vJumpOrigin[3];
float			vJumpAngles[3];

extern "C"
{
void EV_PM_Fall(struct event_args_s *args);
}

void CL_PlaybackEventDirect(int flags, int clientindex, void (*EventFunc)(struct event_args_s *args), int ducking, float *origin, float *angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2);

//#else
//#include "game.h"
#endif // CLIENT_DLL

extern unsigned short g_usPM_Fall;// XDM3035a
static bool pm_shared_initialized = 0;

//extern	int nanmask;
int nanmask = 255<<23;
#define	IS_NAN(x) (((*(int *)&x)&nanmask)==nanmask)


playermove_t *pmove = NULL;
bool g_onladder = 0;

static vec3_t rgv3tStuckTable[54];
static int rgStuckLast[MAX_CLIENTS][2];


// Texture names
static int gcTextures = 0;
static char grgszTextureName[CTEXTURESMAX][CBTEXTURENAMEMAX];
static char grgchTextureType[CTEXTURESMAX];
char *materialfile = "maps/materials.txt";

void PM_SwapTextures(const int &i, const int &j)
{
	char chTemp;
	char szTemp[ CBTEXTURENAMEMAX ];

	strcpy( szTemp, grgszTextureName[i] );
	chTemp = grgchTextureType[i];

	strcpy( grgszTextureName[i], grgszTextureName[j] );
	grgchTextureType[i] = grgchTextureType[j];

	strcpy( grgszTextureName[j], szTemp );
	grgchTextureType[j] = chTemp;
}

void PM_SortTextures(void)
{
	// Bubble sort, yuck, but this only occurs at startup and it's only 512 elements...
	int i, j;
	for (i = 0 ; i < gcTextures; ++i)
	{
		for (j = i + 1; j < gcTextures; ++j)
		{
			if ( stricmp( grgszTextureName[i], grgszTextureName[j] ) > 0 )
				PM_SwapTextures(i, j);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Load data from materialfile
//-----------------------------------------------------------------------------
void PM_InitTextureTypes(void)
{
	char buffer[512];
	int i, j;
	byte *pMemFile;
	int fileSize, filePos;
	static qboolean bTextureTypeInit = false;

	if (bTextureTypeInit)
		return;

	memset(&(grgszTextureName[0][0]), 0, CTEXTURESMAX * CBTEXTURENAMEMAX);
	memset(grgchTextureType, 0, CTEXTURESMAX);

	gcTextures = 0;
	memset(buffer, 0, 512);

	fileSize = pmove->COM_FileSize(materialfile);
	pMemFile = pmove->COM_LoadFile(materialfile, 5, NULL);
	if (!pMemFile)
		return;

	filePos = 0;
	// for each line in the file...
	while (pmove->memfgets(pMemFile, fileSize, &filePos, buffer, 511) != NULL && (gcTextures < CTEXTURESMAX))
	{
		// skip whitespace
		i = 0;
		while(buffer[i] && isspace(buffer[i]))
			++i;

		if (!buffer[i])
			continue;

		// skip comment lines
		if (buffer[i] == '/' || !isalpha(buffer[i]))
			continue;

		// get texture type
		grgchTextureType[gcTextures] = toupper(buffer[i++]);

		// skip whitespace
		while(buffer[i] && isspace(buffer[i]))
			++i;

		if (!buffer[i])
			continue;

		// get sentence name
		j = i;
		while (buffer[j] && !isspace(buffer[j]))
			++j;

		if (!buffer[j])
			continue;

		// null-terminate name and save in sentences array
		j = min(j, CBTEXTURENAMEMAX-1+i);// XDM3036: __min
		buffer[j] = 0;
		strcpy(&(grgszTextureName[gcTextures][0]), &(buffer[i]));
//		strcpy(grgszTextureName[gcTextures++], buffer+i);// ???
//		pmove->Con_DPrintf("parsed: %s (%c)\n", grgszTextureName[gcTextures], grgchTextureType[gcTextures]);
		gcTextures++;
	}

	// Must use engine to free since we are in a .dll
	pmove->COM_FreeFile(pMemFile);
	pmove->Con_DPrintf("PM_InitTextureTypes: %d materials parsed (max %d)\n", gcTextures, CTEXTURESMAX);
//	pmove->Con_DPrintf("PM_InitTextureTypes: %d materials parsed (last: %s)\n", gcTextures, grgszTextureName[gcTextures-1]);

	PM_SortTextures();

	bTextureTypeInit = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
// Output : char
//-----------------------------------------------------------------------------
char PM_FindTextureType(char *name)
{
	int left, right, pivot;
	int val;

	ASSERT(pm_shared_initialized);

	left = 0;
	right = gcTextures - 1;

	while ( left <= right )
	{
		pivot = ( left + right ) / 2;

		val = strnicmp( name, grgszTextureName[ pivot ], CBTEXTURENAMEMAX-1 );
		if ( val == 0 )
		{
			return grgchTextureType[ pivot ];
		}
		else if ( val > 0 )
		{
			left = pivot + 1;
		}
		else if ( val < 0 )
		{
			right = pivot - 1;
		}
	}

	return CHAR_TEX_CONCRETE;
}

//-----------------------------------------------------------------------------
// Purpose: Play gStepSounds
// Input  : &step - 
//			&fvol - 
//-----------------------------------------------------------------------------
void PM_PlayStepSound(const int &step, const float &fvol)
{
	int irand;
	vec3_t hvel;

	pmove->iStepLeft = !pmove->iStepLeft;

	if ( !pmove->runfuncs )
		return;

	irand = pmove->RandomLong(0,1) + ( pmove->iStepLeft * 2);

	// FIXME mp_footsteps needs to be a movevar
	if ( pmove->multiplayer && !pmove->movevars->footsteps )
		return;

	VectorCopy( pmove->velocity, hvel );
	hvel[2] = 0.0;

	if ( pmove->multiplayer && ( !g_onladder && Length( hvel ) <= 220 ) )
		return;

	// irand - 0,1 for right foot, 2,3 for left foot
	// used to alternate left and right foot
	// FIXME, move to player state

//	StepSound = g_step_sounds[step].sounds[irand];
	pmove->PM_PlaySound(CHAN_BODY, gStepSounds[step][irand], fvol, ATTN_NORM, 0, PITCH_NORM);// XDM3035a: :p
}

//-----------------------------------------------------------------------------
// Purpose: Determine texture info for the texture we are standing on.
//-----------------------------------------------------------------------------
void PM_CatagorizeTextureType( void )
{
	vec3_t start, end;
	const char *pTextureName;

	VectorCopy( pmove->origin, start );
	VectorCopy( pmove->origin, end );

	// Straight down
	end[2] -= 64;

	// Fill in default values, just in case.
	pmove->sztexturename[0] = '\0';
	pmove->chtexturetype = CHAR_TEX_CONCRETE;

	pTextureName = pmove->PM_TraceTexture( pmove->onground, start, end );
	if ( !pTextureName )
		return;

	// strip leading '-0' or '+0~' or '{' or '!'
	if (*pTextureName == '-' || *pTextureName == '+')
		pTextureName += 2;

	if (*pTextureName == '{' || *pTextureName == '!' || *pTextureName == '~' || *pTextureName == ' ')
		pTextureName++;
	// '}}'

	strcpy( pmove->sztexturename, pTextureName);
	pmove->sztexturename[ CBTEXTURENAMEMAX - 1 ] = 0;

	// get texture type
	pmove->chtexturetype = PM_FindTextureType( pmove->sztexturename );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void PM_UpdateStepSound(void)
{
	if ( pmove->flTimeStepSound > 0 )
		return;

	if ( pmove->flags & FL_FROZEN )
		return;

	int	fWalking;
	float fvol;
	vec3_t knee;
	vec3_t feet;
	vec3_t center;
	float height;
	float speed;
	float velrun;
	float velwalk;
	int flduck;
	bool fLadder;
	int step;

	PM_CatagorizeTextureType();

	speed = Length( pmove->velocity );
	// determine if we are on a ladder
	fLadder = g_onladder;//( pmove->movetype == MOVETYPE_FLY );// IsOnLadder();

	// UNDONE: need defined numbers for run, walk, crouch, crouch run velocities!!!!
	if ( (pmove->flags & FL_DUCKING) || fLadder )
	{
		velwalk = 60;		// These constants should be based on cl_movespeedkey * cl_forwardspeed somehow
		velrun = 80;		// UNDONE: Move walking to server
		flduck = 100;
	}
	else
	{
		velwalk = 120;
		velrun = 210;
		flduck = 0;
	}

	// If we're on a ladder or on the ground, and we're moving fast enough,
	//  play step sound.  Also, if pmove->flTimeStepSound is zero, get the new
	//  sound right away - we just started moving in new level.
	if ((fLadder || (pmove->onground != -1)) &&
		(/*Length(pmove->velocity )*/speed > 0.0f) &&
		(speed >= velwalk || !pmove->flTimeStepSound))
	{
		fWalking = speed < velrun;

		VectorCopy( pmove->origin, center );
		VectorCopy( pmove->origin, knee );
		VectorCopy( pmove->origin, feet );

		height = pmove->player_maxs[pmove->usehull][2] - pmove->player_mins[pmove->usehull][2];

		knee[2] = pmove->origin[2] - 0.3f * height;
		feet[2] = pmove->origin[2] - 0.5f * height;

		// find out what we're stepping in or on...
		if (fLadder)
		{
			step = STEP_LADDER;
			fvol = 0.35f;
			pmove->flTimeStepSound = 350;
		}
		else if (pmove->PM_PointContents(knee, NULL) <= CONTENTS_WATER)
		{
			step = STEP_WADE;
			fvol = 0.65f;
			pmove->flTimeStepSound = 600;
		}
		else if (pmove->PM_PointContents(feet, NULL) <= CONTENTS_WATER)
		{
			step = STEP_SLOSH;
			fvol = fWalking ? 0.2f : 0.5f;
			pmove->flTimeStepSound = fWalking ? 400 : 300;
		}
		else
		{
			// find texture under player, if different from current texture,
			// get material type
			step = MapTextureTypeStepType(pmove->chtexturetype);

			switch (pmove->chtexturetype)
			{
			default:
			case CHAR_TEX_CONCRETE:
				fvol = fWalking ? 0.2f : 0.5f;
				break;
			case CHAR_TEX_EMPTY:
				fvol = fWalking ? 0.2f : 0.5f;
			break;
			case CHAR_TEX_ASPHALT:
				fvol = fWalking ? 0.2f : 0.5f;
				break;
			case CHAR_TEX_BRICK:
				fvol = fWalking ? 0.2f : 0.5f;
				break;
			case CHAR_TEX_ROCK:
				fvol = fWalking ? 0.2f : 0.5f;
			break;
			case CHAR_TEX_SAND_ROCK:
				fvol = fWalking ? 0.2f : 0.5f;
			break;
			case CHAR_TEX_ICE:
				fvol = fWalking ? 0.2f : 0.5f;
			break;
			case CHAR_TEX_METAL:
				fvol = fWalking ? 0.2f : 0.5f;
				break;
			case CHAR_TEX_SAND:
				fvol = fWalking ? 0.2f : 0.5f;
				break;
			case CHAR_TEX_VENT:
				fvol = fWalking ? 0.4f : 0.7f;
				break;
			case CHAR_TEX_GRATE:
				fvol = fWalking ? 0.2f : 0.5f;
				break;
			case CHAR_TEX_TILE:
				fvol = fWalking ? 0.2f : 0.5f;
				break;
			case CHAR_TEX_SLOSH:
				fvol = fWalking ? 0.2f : 0.5f;
				break;
			case CHAR_TEX_WOOD:
				fvol = fWalking ? 0.2f : 0.5f;
				break;
			case CHAR_TEX_COMPUTER:
				fvol = fWalking ? 0.2f : 0.4f;
				break;
			case CHAR_TEX_FLESH:
				fvol = fWalking ? 0.1f : 0.3f;
				break;
			case CHAR_TEX_SNOW:
				fvol = fWalking ? 0.2f : 0.5f;
				break;
			case CHAR_TEX_GRASS:
				fvol = fWalking ? 0.2f : 0.5f;
				break;
			case CHAR_TEX_LEAVES:
				fvol = fWalking ? 0.2f : 0.5f;
				break;
			case CHAR_TEX_ENERGYSHIELD:
				fvol = fWalking ? 0.4f : 0.8f;
				break;

			}
			pmove->flTimeStepSound = fWalking ? 400 : 300;// XDM: same for all
		}

		pmove->flTimeStepSound += flduck; // slower step time if ducking
		// play the sound
		// 35% volume if ducking
		if ( pmove->flags & FL_DUCKING )
			fvol *= 0.35f;

		PM_PlayStepSound( step, fvol );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Adds the trace result to touch list, if contact is not already in list.
// Input  : tr - 
//			&impactvelocity - 
// Output : qboolean
//-----------------------------------------------------------------------------
qboolean PM_AddToTouched(pmtrace_t tr, vec3_t &impactvelocity)
{
	int i;
	for (i = 0; i < pmove->numtouch; ++i)
	{
		if (pmove->touchindex[i].ent == tr.ent)
			break;
	}
	if (i != pmove->numtouch)  // Already in list.
		return false;

	VectorCopy( impactvelocity, tr.deltavelocity );

	if (pmove->numtouch >= MAX_PHYSENTS)
		pmove->Con_DPrintf("PM(%d): Too many entities were touched!\n", pmove->player_index);

	pmove->touchindex[pmove->numtouch++] = tr;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: See if the player has a bogus velocity value.
//-----------------------------------------------------------------------------
void PM_CheckVelocity(void)
{
	int		i;
//
// bound velocity
//
	for (i=0 ; i<3 ; ++i)
	{
		// See if it's bogus.
		if (IS_NAN(pmove->velocity[i]))
		{
			pmove->Con_Printf ("PM  Got a NaN velocity %i\n", i);
			pmove->velocity[i] = 0;
		}
		if (IS_NAN(pmove->origin[i]))
		{
			pmove->Con_Printf ("PM  Got a NaN origin on %i\n", i);
			pmove->origin[i] = 0;
		}
		// Bound it.
		if (pmove->velocity[i] > pmove->movevars->maxvelocity)
		{
			pmove->Con_DPrintf ("PM  Got a velocity too high on %i\n", i);
			pmove->velocity[i] = pmove->movevars->maxvelocity;
		}
		else if (pmove->velocity[i] < -pmove->movevars->maxvelocity)
		{
			pmove->Con_DPrintf ("PM  Got a velocity too low on %i\n", i);
			pmove->velocity[i] = -pmove->movevars->maxvelocity;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Slide off of the impacting object
// Input  : in - 
//			normal - 
//			*out - 
//			&overbounce - 
// Output : int blocked flags: 0x01 == floor, 0x02 == step / wall
//-----------------------------------------------------------------------------
int PM_ClipVelocity(vec3_t in, vec3_t normal, float *out, const float &overbounce)
{
	float	backoff;
	float	change;
	float angle;
	int		i, blocked;

	angle = normal[ 2 ];

	blocked = 0x00;            // Assume unblocked.
	if (angle > 0)      // If the plane that is blocking us has a positive z component, then assume it's a floor.
		blocked |= 0x01;		//
	if (!angle)         // If the plane has no Z, it is vertical (wall/step)
		blocked |= 0x02;		//

	// Determine how far along plane to slide based on incoming direction.
	// Scale by overbounce factor.
	backoff = DotProduct (in, normal) * overbounce;

	for (i=0; i<3; ++i)
	{
		change = normal[i]*backoff;
		out[i] = in[i] - change;
		// If out velocity is too small, zero it out.
		if (out[i] > -STOP_EPSILON && out[i] < STOP_EPSILON)
			out[i] = 0;
	}

	// Return blocking flags.
	return blocked;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void PM_AddCorrectGravity(void)
{
	if (pmove->waterjumptime)
		return;

	float	ent_gravity;
	if (pmove->gravity)
		ent_gravity = pmove->gravity;
	else
		ent_gravity = 1.0;

	// Add gravity so they'll be in the correct position during movement
	// yes, this 0.5 looks wrong, but it's not.
	pmove->velocity[2] -= (ent_gravity * pmove->movevars->gravity * pmove->frametime * 0.5f );
	pmove->velocity[2] += pmove->basevelocity[2] * pmove->frametime;
	pmove->basevelocity[2] = 0;

	PM_CheckVelocity();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void PM_FixupGravityVelocity(void)
{
	if (pmove->waterjumptime)
		return;

	float	ent_gravity;
	if (pmove->gravity)
		ent_gravity = pmove->gravity;
	else
		ent_gravity = 1.0;

	// Get the correct velocity for the end of the dt
  	pmove->velocity[2] -= (ent_gravity * pmove->movevars->gravity * pmove->frametime * 0.5f );

	PM_CheckVelocity();
}

//-----------------------------------------------------------------------------
// Purpose: The basic solid body movement clip that slides along multiple planes
// Output : int
//-----------------------------------------------------------------------------
int PM_FlyMove(void)
{
	int			bumpcount, numbumps;
	vec3_t		dir;
	float		d;
	int			numplanes;
	vec3_t		planes[MAX_CLIP_PLANES];
	vec3_t		primal_velocity, original_velocity;
	vec3_t      new_velocity;
	int			i, j;
	pmtrace_t	trace;
	vec3_t		end;
	float		time_left, allFraction;
	int			blocked;

	numbumps  = 4;           // Bump up to four times
	blocked   = 0;           // Assume not blocked
	numplanes = 0;           //  and not sliding along any planes

	VectorCopy (pmove->velocity, original_velocity);  // Store original velocity
	VectorCopy (pmove->velocity, primal_velocity);

	allFraction = 0;
	time_left = pmove->frametime;   // Total time for this movement operation.

	for (bumpcount=0; bumpcount<numbumps; ++bumpcount)
	{
		if (!pmove->velocity[0] && !pmove->velocity[1] && !pmove->velocity[2])
			break;

		// Assume we can move all the way from the current origin to the
		//  end point.
		for (i=0; i<3; ++i)
			end[i] = pmove->origin[i] + time_left * pmove->velocity[i];

		// See if we can make it from origin to end point.
		trace = pmove->PM_PlayerTrace (pmove->origin, end, PM_NORMAL, -1 );

		allFraction += trace.fraction;
		// If we started in a solid object, or we were in solid space
		//  the whole way, zero out our velocity and return that we
		//  are blocked by floor and wall.
		if (trace.allsolid)
		{	// entity is trapped in another solid
			VectorCopy(g_vecZero, pmove->velocity);
			//Con_DPrintf("Trapped 4\n");
			return 4;
		}

		// If we moved some portion of the total distance, then
		//  copy the end position into the pmove->origin and
		//  zero the plane counter.
		if (trace.fraction > 0)
		{	// actually covered some distance
			VectorCopy (trace.endpos, pmove->origin);
			VectorCopy (pmove->velocity, original_velocity);
			numplanes = 0;
		}

		// If we covered the entire distance, we are done
		//  and can return.
		if (trace.fraction == 1)
			 break;		// moved the entire distance

		//if (!trace.ent)
		//	Sys_Error ("PM_PlayerTrace: !trace.ent");

		// Save entity that blocked us (since fraction was < 1.0)
		//  for contact
		// Add it if it's not already in the list!!!
		PM_AddToTouched(trace, pmove->velocity);

		// If the plane we hit has a high z component in the normal, then
		//  it's probably a floor
		if (trace.plane.normal[2] > 0.7)
		{
			blocked |= 1;		// floor
		}
		// If the plane has a zero z component in the normal, then it's a
		//  step or wall
		if (!trace.plane.normal[2])
		{
			blocked |= 2;		// step / wall
			//Con_DPrintf("Blocked by %i\n", trace.ent);
		}

		// Reduce amount of pmove->frametime left by total time left * fraction
		//  that we covered.
		time_left -= time_left * trace.fraction;

		// Did we run out of planes to clip against?
		if (numplanes >= MAX_CLIP_PLANES)
		{	// this shouldn't really happen
			//  Stop our movement if so.
			VectorCopy (g_vecZero, pmove->velocity);
			//Con_DPrintf("Too many planes 4\n");
			break;
		}

		// Set up next clipping plane
		VectorCopy (trace.plane.normal, planes[numplanes]);
		numplanes++;

// modify original_velocity so it parallels all of the clip planes
		if ( pmove->movetype == MOVETYPE_WALK &&
			((pmove->onground == -1) || (pmove->friction != 1)) )	// relfect player velocity
		{
			for (i = 0; i < numplanes; ++i)
			{
				if (planes[i][2] > 0.7)
				{// floor or slope
					PM_ClipVelocity( original_velocity, planes[i], new_velocity, 1 );
					VectorCopy( new_velocity, original_velocity );
				}
				else
					PM_ClipVelocity( original_velocity, planes[i], new_velocity, 1.0f + pmove->movevars->bounce * (1-pmove->friction) );
			}

			VectorCopy( new_velocity, pmove->velocity );
			VectorCopy( new_velocity, original_velocity );
		}
		else
		{
			for (i=0 ; i<numplanes ; ++i)
			{
				PM_ClipVelocity(original_velocity, planes[i], pmove->velocity, 1);
				for (j=0 ; j<numplanes ; ++j)
				{
					if (j != i)
					{
						// Are we now moving against this plane?
						if (DotProduct (pmove->velocity, planes[j]) < 0)
							break;	// not ok
					}
				}
				if (j == numplanes)  // Didn't have to clip, so we're ok
					break;
			}

			// Did we go all the way through plane set
			if (i != numplanes)
			{	// go along this plane
				// pmove->velocity is set in clipping call, no need to set again.
				;
			}
			else
			{	// go along the crease
				if (numplanes != 2)
				{
					//Con_Printf ("clip velocity, numplanes == %i\n",numplanes);
					VectorCopy (g_vecZero, pmove->velocity);
					//Con_DPrintf("Trapped 4\n");

					break;
				}
				CrossProduct (planes[0], planes[1], dir);
				d = DotProduct (dir, pmove->velocity);
				VectorScale (dir, d, pmove->velocity );
			}

	//
	// if original velocity is against the original velocity, stop dead
	// to avoid tiny occilations in sloping corners
	//
			if (DotProduct (pmove->velocity, primal_velocity) <= 0)
			{
				//Con_DPrintf("Back\n");
				VectorCopy (g_vecZero, pmove->velocity);
				break;
			}
		}
	}

	if ( allFraction == 0 )
	{
		VectorCopy (g_vecZero, pmove->velocity);
		//Con_DPrintf( "Don't stick\n" );
	}

	return blocked;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *wishdir - 
//			&wishspeed - 
//			&accel - 
//-----------------------------------------------------------------------------
void PM_Accelerate(const vec3_t &wishdir, const float &wishspeed, const float &accel)
{
	int			i;
	float		addspeed, accelspeed, currentspeed;

	// Dead player's don't accelerate
	if (pmove->dead)
		return;

	// If waterjumping, don't accelerate
	if (pmove->waterjumptime)
		return;

	// See if we are changing direction a bit
	currentspeed = DotProduct(pmove->velocity, wishdir);

	// Reduce wishspeed by the amount of veer.
	addspeed = wishspeed - currentspeed;

	// If not going to add any speed, done.
	if (addspeed <= 0)
		return;

	// Determine amount of accleration.
	accelspeed = accel * pmove->frametime * wishspeed * pmove->friction;

	// Cap at addspeed
	if (accelspeed > addspeed)
		accelspeed = addspeed;

	// Adjust velocity.
	for (i=0 ; i<3 ; ++i)
		pmove->velocity[i] += accelspeed * wishdir[i];
}

//-----------------------------------------------------------------------------
// Purpose: Only used by players.  Moves along the ground when player is a MOVETYPE_WALK.
//-----------------------------------------------------------------------------
void PM_WalkMove(void)
{
	int			clip;
	int			oldonground;
	int i;

	vec3_t		wishvel;
	float       spd;
	float		fmove, smove;
	vec3_t		wishdir;
	float		wishspeed;

	vec3_t dest, start;
	vec3_t original, originalvel;
	vec3_t down, downvel;
	float downdist, updist;

	pmtrace_t trace;

	// Copy movement amounts
	fmove = pmove->cmd.forwardmove;
	smove = pmove->cmd.sidemove;

	// Zero out z components of movement vectors
	pmove->forward[2] = 0;
	pmove->right[2]   = 0;

	VectorNormalize (pmove->forward);  // Normalize remainder of vectors.
	VectorNormalize (pmove->right);    //

	for (i=0 ; i<2 ; ++i)       // Determine x and y parts of velocity
		wishvel[i] = pmove->forward[i]*fmove + pmove->right[i]*smove;

	wishvel[2] = 0;             // Zero out z part of velocity

	VectorCopy (wishvel, wishdir);   // Determine maginitude of speed of move
	wishspeed = VectorNormalize(wishdir);
//
// Clamp to server defined max speed
//
	if (wishspeed > pmove->maxspeed)
	{
		VectorScale (wishvel, pmove->maxspeed/wishspeed, wishvel);
		wishspeed = pmove->maxspeed;
	}
	bool cansuperrun = atoi(pmove->PM_Info_ValueForKey(pmove->physinfo, PHYSKEY_HASTE)) == 1?true:false;

	if (cansuperrun)
		wishspeed *= 2;

	// Set pmove velocity
	pmove->velocity[2] = 0;
	PM_Accelerate (wishdir, wishspeed, pmove->movevars->accelerate);
	pmove->velocity[2] = 0;

	// Add in any base velocity to the current velocity.
	VectorAdd (pmove->velocity, pmove->basevelocity, pmove->velocity );

	spd = Length(pmove->velocity);

	if (spd < 1.0f)
	{
		VectorClear( pmove->velocity );
		return;
	}

	// If we are not moving, do nothing
	//if (!pmove->velocity[0] && !pmove->velocity[1] && !pmove->velocity[2])
	//	return;

	oldonground = pmove->onground;

// first try just moving to the destination
	dest[0] = pmove->origin[0] + pmove->velocity[0]*pmove->frametime;
	dest[1] = pmove->origin[1] + pmove->velocity[1]*pmove->frametime;
	dest[2] = pmove->origin[2];

	// first try moving directly to the next spot
	VectorCopy (dest, start);
	trace = pmove->PM_PlayerTrace (pmove->origin, dest, PM_NORMAL, -1 );
	// If we made it all the way, then copy trace end
	//  as new player position.
	if (trace.fraction == 1)
	{
		VectorCopy (trace.endpos, pmove->origin);
		return;
	}

	if (oldonground == -1 &&   // Don't walk up stairs if not on ground.
		pmove->waterlevel  == 0)
		return;

	if (pmove->waterjumptime)         // If we are jumping out of water, don't do anything more.
		return;

	// Try sliding forward both on ground and up 16 pixels
	//  take the move that goes farthest
	VectorCopy (pmove->origin, original);       // Save out original pos &
	VectorCopy (pmove->velocity, originalvel);  //  velocity.

	// Slide move
	clip = PM_FlyMove ();

	// Copy the results out
	VectorCopy (pmove->origin  , down);
	VectorCopy (pmove->velocity, downvel);

	// Reset original values.
	VectorCopy (original, pmove->origin);
	VectorCopy (originalvel, pmove->velocity);

	// Start out up one stair height
	VectorCopy (pmove->origin, dest);
	dest[2] += pmove->movevars->stepsize;

	trace = pmove->PM_PlayerTrace(pmove->origin, dest, PM_NORMAL, -1);
	// If we started okay and made it part of the way at least,
	//  copy the results to the movement start position and then
	//  run another move try.
	if (!trace.startsolid && !trace.allsolid)
	{
		VectorCopy (trace.endpos, pmove->origin);
	}

// slide move the rest of the way.
	clip = PM_FlyMove ();

// Now try going back down from the end point
//  press down the stepheight
	VectorCopy (pmove->origin, dest);
	dest[2] -= pmove->movevars->stepsize;

	trace = pmove->PM_PlayerTrace(pmove->origin, dest, PM_NORMAL, -1);

	// If we are not on the ground any more then
	//  use the original movement attempt
	if ( trace.plane.normal[2] < 0.7)
		goto usedown;
	// If the trace ended up in empty space, copy the end
	//  over to the origin.
	if (!trace.startsolid && !trace.allsolid)
	{
		VectorCopy (trace.endpos, pmove->origin);
	}
	// Copy this origion to up.
	VectorCopy (pmove->origin, pmove->up);

	// decide which one went farther
	downdist = (down[0] - original[0])*(down[0] - original[0])
		     + (down[1] - original[1])*(down[1] - original[1]);
	updist   = (pmove->up[0]   - original[0])*(pmove->up[0]   - original[0])
		     + (pmove->up[1]   - original[1])*(pmove->up[1]   - original[1]);

	if (downdist > updist)
	{
usedown:
		VectorCopy (down   , pmove->origin);
		VectorCopy (downvel, pmove->velocity);
	} else // copy z value from slide move
		pmove->velocity[2] = downvel[2];

}

//-----------------------------------------------------------------------------
// Purpose: Handles both ground friction and water friction
//-----------------------------------------------------------------------------
void PM_Friction(void)
{
	float	*vel;
	float	speed, newspeed, control;
	float	friction;
	float	drop;
	vec3_t newvel;

	// If we are in water jump cycle, don't apply friction
	if (pmove->waterjumptime)
		return;

	// Get velocity
	vel = pmove->velocity;

	// Calculate speed
	speed = sqrtf(vel[0]*vel[0] +vel[1]*vel[1] + vel[2]*vel[2]);

	// If too slow, return
	if (speed < 0.1f)
		return;

	drop = 0;

// apply ground friction
	if (pmove->onground != -1)  // On an entity that is the ground
	{
		vec3_t start, stop;
		pmtrace_t trace;

		start[0] = stop[0] = pmove->origin[0] + vel[0]/speed*16;
		start[1] = stop[1] = pmove->origin[1] + vel[1]/speed*16;
		start[2] = pmove->origin[2] + pmove->player_mins[pmove->usehull][2];
		stop[2] = start[2] - 34;

		trace = pmove->PM_PlayerTrace(start, stop, PM_NORMAL, -1);

		if (trace.fraction == 1.0)
			friction = pmove->movevars->friction*pmove->movevars->edgefriction;
		else
			friction = pmove->movevars->friction;

		// Grab friction value.
		//friction = pmove->movevars->friction;

		friction *= pmove->friction;  // player friction?

		// Bleed off some speed, but if we have less than the bleed
		//  threshhold, bleed the theshold amount.
		control = (speed < pmove->movevars->stopspeed) ?
			pmove->movevars->stopspeed : speed;
		// Add the amount to t'he drop amount.
		drop += control*friction*pmove->frametime;
	}

// apply water friction
//	if (pmove->waterlevel)
//		drop += speed * pmove->movevars->waterfriction * waterlevel * pmove->frametime;

// scale the velocity
	newspeed = speed - drop;
	if (newspeed < 0)
		newspeed = 0;

	// Determine proportion of old speed we are using.
	newspeed /= speed;

	// Adjust velocity according to proportion.
	newvel[0] = vel[0] * newspeed;
	newvel[1] = vel[1] * newspeed;
	newvel[2] = vel[2] * newspeed;

	VectorCopy( newvel, pmove->velocity );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : wishdir - 
//			wishspeed - 
//			accel - 
//-----------------------------------------------------------------------------
void PM_AirAccelerate(const Vector &wishdir, const float &wishspeed, const float &accel)
{
//	int			i;
	float		addspeed, accelspeed, currentspeed, wishspd = wishspeed;

	if (pmove->dead)
		return;
	if (pmove->waterjumptime)
		return;

	// Cap speed
	//wishspd = VectorNormalize (pmove->wishveloc);

	if (wishspd > 30)
		wishspd = 30;
	// Determine veer amount
	currentspeed = DotProduct (pmove->velocity, wishdir);
	// See how much to add
	addspeed = wishspd - currentspeed;
	// If not adding any, done.
	if (addspeed <= 0)
		return;
	// Determine acceleration speed after acceleration

	accelspeed = accel * wishspeed * pmove->frametime * pmove->friction;
	// Cap it
	if (accelspeed > addspeed)
		accelspeed = addspeed;

	// Adjust pmove vel.
//	for (i=0 ; i<3 ; ++i)
//		pmove->velocity[i] += accelspeed*wishdir[i];
	//pmove->velocity = pmove->velocity+accelspeed*wishdir;
	VectorMA(pmove->velocity, accelspeed, wishdir, pmove->velocity);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void PM_WaterMove(void)
{
	int		i;
	vec3_t	wishvel;
	float	wishspeed;
	vec3_t	wishdir;
	vec3_t	start, dest;
	vec3_t  temp;
	pmtrace_t	trace;
	float speed, newspeed, addspeed, accelspeed;

//
// user intentions
//
	for (i=0 ; i<3 ; i++)
		wishvel[i] = pmove->forward[i]*pmove->cmd.forwardmove + pmove->right[i]*pmove->cmd.sidemove;

	// Sinking after no other movement occurs
	if (!pmove->cmd.forwardmove && !pmove->cmd.sidemove && !pmove->cmd.upmove)
		wishvel[2] -= 60;		// drift towards bottom
	else  // Go straight up by upmove amount.
		wishvel[2] += pmove->cmd.upmove;

	// Copy it over and determine speed
	VectorCopy (wishvel, wishdir);
	wishspeed = VectorNormalize(wishdir);

	// Cap speed.
	if (wishspeed > pmove->maxspeed)
	{
		VectorScale (wishvel, pmove->maxspeed/wishspeed, wishvel);
		wishspeed = pmove->maxspeed;
	}
	// Slow us down a bit.
	wishspeed *= 0.8f;

	VectorAdd (pmove->velocity, pmove->basevelocity, pmove->velocity);
// Water friction
	VectorCopy(pmove->velocity, temp);
	speed = VectorNormalize(temp);
	if (speed)
	{
		newspeed = speed - pmove->frametime * speed * pmove->movevars->friction * pmove->friction;

		if (newspeed < 0)
			newspeed = 0;
		VectorScale (pmove->velocity, newspeed/speed, pmove->velocity);
	}
	else
		newspeed = 0;

//
// water acceleration
//
	if ( wishspeed < 0.1f )
		return;

	addspeed = wishspeed - newspeed;
	if (addspeed > 0)
	{

		VectorNormalize(wishvel);
		accelspeed = pmove->movevars->accelerate * wishspeed * pmove->frametime * pmove->friction;
		if (accelspeed > addspeed)
			accelspeed = addspeed;

		for (i = 0; i < 3; i++)
			pmove->velocity[i] += accelspeed * wishvel[i];
	}

// Now move
// assume it is a stair or a slope, so press down from stepheight above
	VectorMA (pmove->origin, pmove->frametime, pmove->velocity, dest);
	VectorCopy (dest, start);
	start[2] += pmove->movevars->stepsize + 1;
	trace = pmove->PM_PlayerTrace (start, dest, PM_NORMAL, -1 );
	if (!trace.startsolid && !trace.allsolid)	// FIXME: check steep slope?
	{	// walked up the step, so just keep result and exit
		VectorCopy (trace.endpos, pmove->origin);
		return;
	}

	// Try moving straight along out normal path.
	PM_FlyMove();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void PM_AirMove(void)
{
	int			i;
	vec3_t		wishvel;
	float		fmove, smove;
	vec3_t		wishdir;
	float		wishspeed;

	// Copy movement amounts
	fmove = pmove->cmd.forwardmove;
	smove = pmove->cmd.sidemove;

	// Zero out z components of movement vectors
	pmove->forward[2] = 0;
	pmove->right[2]   = 0;
	// Renormalize
	VectorNormalize (pmove->forward);
	VectorNormalize (pmove->right);

	// Determine x and y parts of velocity
	for (i=0 ; i<2 ; i++)
		wishvel[i] = pmove->forward[i]*fmove + pmove->right[i]*smove;
	// Zero out z part of velocity
	wishvel[2] = 0;

	 // Determine maginitude of speed of move
	VectorCopy (wishvel, wishdir);
	wishspeed = VectorNormalize(wishdir);

	// Clamp to server defined max speed
	if (wishspeed > pmove->maxspeed)
	{
		VectorScale (wishvel, pmove->maxspeed/wishspeed, wishvel);
		wishspeed = pmove->maxspeed;
	}

	PM_AirAccelerate (wishdir, wishspeed, pmove->movevars->airaccelerate);

	// Add in any base velocity to the current velocity.
	VectorAdd(pmove->velocity, pmove->basevelocity, pmove->velocity);

	PM_FlyMove();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool PM_InWater(void)
{
	return (pmove->waterlevel > 1);
}

//-----------------------------------------------------------------------------
// Purpose: Sets pmove->waterlevel and pmove->watertype values.
// Output : qboolean
//-----------------------------------------------------------------------------
qboolean PM_CheckWater(void)
{
	vec3_t	point;
	int		cont;
	int		truecont;
	float	height;
	float	heightover2;

	// Pick a spot just above the players feet.
	point[0] = pmove->origin[0] + (pmove->player_mins[pmove->usehull][0] + pmove->player_maxs[pmove->usehull][0]) * 0.5f;
	point[1] = pmove->origin[1] + (pmove->player_mins[pmove->usehull][1] + pmove->player_maxs[pmove->usehull][1]) * 0.5f;
	point[2] = pmove->origin[2] + pmove->player_mins[pmove->usehull][2] + 1.0f;

	// Assume that we are not in water at all.
	pmove->waterlevel = 0;
	pmove->watertype = CONTENTS_EMPTY;

	// Grab point contents.
	cont = pmove->PM_PointContents (point, &truecont );
	// Are we under water? (not solid and not empty?)
	if (cont <= CONTENTS_WATER && cont > CONTENTS_TRANSLUCENT )
	{
		// Set water type
		pmove->watertype = cont;

		// We are at least at level one
		pmove->waterlevel = 1;

		height = (pmove->player_mins[pmove->usehull][2] + pmove->player_maxs[pmove->usehull][2]);
		heightover2 = height * 0.5f;

		// Now check a point that is at the player hull midpoint.
		point[2] = pmove->origin[2] + heightover2;
		cont = pmove->PM_PointContents (point, NULL );
		// If that point is also under water...
		if (cont <= CONTENTS_WATER && cont > CONTENTS_TRANSLUCENT )
		{
			// Set a higher water level.
			pmove->waterlevel = 2;

			// Now check the eye position.  (view_ofs is relative to the origin)
			point[2] = pmove->origin[2] + pmove->view_ofs[2];

			cont = pmove->PM_PointContents (point, NULL );
			if (cont <= CONTENTS_WATER && cont > CONTENTS_TRANSLUCENT )
				pmove->waterlevel = 3;  // In over our eyes
		}

		// Adjust velocity based on water current, if any.
		if ((truecont <= CONTENTS_CURRENT_0) &&
			(truecont >= CONTENTS_CURRENT_DOWN))
		{
			// The deeper we are, the stronger the current.
			static float current_table[][3] =
			{
				{1, 0, 0}, {0, 1, 0}, {-1, 0, 0},
				{0, -1, 0}, {0, 0, 1}, {0, 0, -1}
			};

			VectorMA (pmove->basevelocity, 50.0f*pmove->waterlevel, current_table[CONTENTS_CURRENT_0 - truecont], pmove->basevelocity);
		}
	}

	return pmove->waterlevel > 1;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : void PM_CatagorizePosition
//-----------------------------------------------------------------------------
void PM_CatagorizePosition(void)
{
	vec3_t		point;
	pmtrace_t		tr;

// if the player hull point one unit down is solid, the player
// is on ground

// see if standing on something solid

	// Doing this before we move may introduce a potential latency in water detection, but
	// doing it after can get us stuck on the bottom in water if the amount we move up
	// is less than the 1 pixel 'threshold' we're about to snap to.	Also, we'll call
	// this several times per frame, so we really need to avoid sticking to the bottom of
	// water on each call, and the converse case will correct itself if called twice.
	PM_CheckWater();

	point[0] = pmove->origin[0];
	point[1] = pmove->origin[1];
	point[2] = pmove->origin[2] - 2.0f;

	if (pmove->velocity[2] > 180)   // Shooting up really fast.  Definitely not on ground.
	{
		pmove->onground = -1;
	}
	else
	{
		// Try and move down.
		tr = pmove->PM_PlayerTrace (pmove->origin, point, PM_NORMAL, -1 );
		// If we hit a steep plane, we are not on ground
		if (tr.plane.normal[2] < 0.7)
			pmove->onground = -1;	// too steep
		else
			pmove->onground = tr.ent;  // Otherwise, point to index of ent under us.

		// If we are on something...
		if (pmove->onground != -1)
		{
			// Then we are not in water jump sequence
			pmove->waterjumptime = 0;
			// If we could make the move, drop us down that 1 pixel
			if (pmove->waterlevel < 2 && !tr.startsolid && !tr.allsolid)
				VectorCopy (tr.endpos, pmove->origin);
		}

		// Standing on an entity other than the world
		if (tr.ent > 0)   // So signal that we are touching something.
		{
			PM_AddToTouched(tr, pmove->velocity);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: When a player is stuck, it's costly to try and unstick them
//			Grab a test offset for the player based on a passed in index
// Input  : &nIndex - 
//			&server - 
//			&offset - 
// Output : int
//-----------------------------------------------------------------------------
int PM_GetRandomStuckOffsets(const int &nIndex, const int &server, const Vector &offset)
{
 // Last time we did a full
	int idx;
	idx = rgStuckLast[nIndex][server]++;

	VectorCopy(rgv3tStuckTable[idx % 54], offset);

	return (idx % 54);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &nIndex - 
//			&server - 
//-----------------------------------------------------------------------------
void PM_ResetStuckOffsets(const int &nIndex, const int &server)
{
	rgStuckLast[nIndex][server] = 0;
}


#define PM_CHECKSTUCK_MINTIME 0.05  // Don't check again too quickly.

//-----------------------------------------------------------------------------
// Purpose: If pmove->origin is in a solid position, try nudging slightly on all axis to
//			allow for the cut precision of the net coordinates
// Output : int PM_CheckStuck
//-----------------------------------------------------------------------------
int PM_CheckStuck (void)
{
	vec3_t	base;
	vec3_t  offset;
	vec3_t  test;
	int     hitent;
	int		idx;
	float	fTime;
	int i;
	pmtrace_t traceresult;

	static float rgStuckCheckTime[MAX_CLIENTS][2]; // Last time we did a full

	// If position is okay, exit
	hitent = pmove->PM_TestPlayerPosition (pmove->origin, &traceresult );
	if (hitent == -1 )
	{
		PM_ResetStuckOffsets( pmove->player_index, pmove->server );
		return 0;
	}

	VectorCopy (pmove->origin, base);

	// Deal with precision error in network.
	if (!pmove->server)
	{
		// World or BSP model
		if ((hitent == 0) || (pmove->physents[hitent].model != NULL))
		{
			int nReps = 0;
			PM_ResetStuckOffsets( pmove->player_index, pmove->server );
			do
			{
				i = PM_GetRandomStuckOffsets(pmove->player_index, pmove->server, offset);

				VectorAdd(base, offset, test);
				if (pmove->PM_TestPlayerPosition (test, &traceresult ) == -1)
				{
					PM_ResetStuckOffsets( pmove->player_index, pmove->server );

					VectorCopy ( test, pmove->origin );
					return 0;
				}
				nReps++;
			} while (nReps < 54);
		}
	}

	// Only an issue on the client.
	if (pmove->server)
		idx = 0;
	else
		idx = 1;

	fTime = (float)pmove->Sys_FloatTime();
	// Too soon?
	if (rgStuckCheckTime[pmove->player_index][idx] >= (fTime - PM_CHECKSTUCK_MINTIME))
		return 1;

	rgStuckCheckTime[pmove->player_index][idx] = fTime;

	pmove->PM_StuckTouch( hitent, &traceresult );

	i = PM_GetRandomStuckOffsets(pmove->player_index, pmove->server, offset);

	VectorAdd(base, offset, test);

	if ((hitent = pmove->PM_TestPlayerPosition(test, NULL)) == -1)
	{
		//Con_DPrintf("Nudged\n");
		PM_ResetStuckOffsets( pmove->player_index, pmove->server );

		if (i >= 27)
			VectorCopy(test, pmove->origin);

		return 0;
	}

	// If player is flailing while stuck in another player ( should never happen ), then see
	//  if we can't "unstick" them forceably.
	if ( pmove->cmd.buttons & ( IN_JUMP | IN_DUCK | IN_ATTACK ) && ( pmove->physents[ hitent ].player != 0 ) )
	{
		float x, y, z;
		float xystep = 8.0f;
		float zstep = 18.0f;
		float xyminmax = xystep;
		float zminmax = 4 * zstep;

		for ( z = 0; z <= zminmax; z += zstep )
		{
			for ( x = -xyminmax; x <= xyminmax; x += xystep )
			{
				for ( y = -xyminmax; y <= xyminmax; y += xystep )
				{
					VectorCopy( base, test );
					test[0] += x;
					test[1] += y;
					test[2] += z;

					if ( pmove->PM_TestPlayerPosition ( test, NULL ) == -1 )
					{
						VectorCopy( test, pmove->origin );
						return 0;
					}
				}
			}
		}
	}
	//VectorCopy (base, pmove->origin);
	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: PM_SpectatorMove
//-----------------------------------------------------------------------------
void PM_SpectatorMove(void)
{
	float	speed, drop, friction, control, newspeed;
	//float   accel;
	float	currentspeed, addspeed, accelspeed;
	int			i;
	vec3_t		wishvel;
	float		fmove, smove;
	vec3_t		wishdir;
	float		wishspeed;
	// this routine keeps track of the spectators psoition
	// there a two different main move types : track player or move freely (OBS_ROAMING)
	// doesn't need excate track position, only to generate PVS, so just copy
	// targets position and real view position is calculated on client (saves server CPU)

	if (pmove->iuser1 == OBS_ROAMING)
	{
#ifdef CLIENT_DLL
		// jump only in roaming mode
		if (iJumpSpectator)
		{
			VectorCopy(vJumpOrigin, pmove->origin);
			VectorCopy(vJumpAngles, pmove->angles);
			VectorCopy(g_vecZero, pmove->velocity);
			iJumpSpectator = 0;
			return;
		}
#endif
		// Move around in normal spectator method
		speed = Length(pmove->velocity);
		if (speed < 1)
		{
			VectorCopy(g_vecZero, pmove->velocity);
		}
		else
		{
			drop = 0;

			friction = pmove->movevars->friction*1.5f;	// extra friction
			control = speed < pmove->movevars->stopspeed ? pmove->movevars->stopspeed : speed;
			drop += control*friction*pmove->frametime;

			// scale the velocity
			newspeed = speed - drop;
			if (newspeed < 0)
				newspeed = 0;
			newspeed /= speed;

			VectorScale(pmove->velocity, newspeed, pmove->velocity);
		}

//		PM_NoClip();?
		// accelerate
		fmove = pmove->cmd.forwardmove;
		smove = pmove->cmd.sidemove;

		VectorNormalize(pmove->forward);
		VectorNormalize(pmove->right);

		for (i=0; i<3; ++i)
			wishvel[i] = pmove->forward[i]*fmove + pmove->right[i]*smove;

		wishvel[2] += pmove->cmd.upmove;

		VectorCopy(wishvel, wishdir);
		wishspeed = VectorNormalize(wishdir);

		// clamp to server defined max speed
		if (wishspeed > pmove->movevars->spectatormaxspeed)
		{
			VectorScale (wishvel, pmove->movevars->spectatormaxspeed/wishspeed, wishvel);
			wishspeed = pmove->movevars->spectatormaxspeed;
		}
		currentspeed = DotProduct(pmove->velocity, wishdir);
		addspeed = wishspeed - currentspeed;
		if (addspeed <= 0)
			return;

		accelspeed = pmove->movevars->accelerate*pmove->frametime*wishspeed;
		if (accelspeed > addspeed)
			accelspeed = addspeed;

		for (i=0 ; i<3 ; i++)
			pmove->velocity[i] += accelspeed*wishdir[i];
/*
#ifndef CLIENT_DLL
		extern cvar_t sv_specnoclip;
		if (sv_specnoclip.value <= 0.0f)
			PM_FlyMove();
#endif*/
		// move

		VectorMA(pmove->origin, pmove->frametime, pmove->velocity*SPEED_SPECTATOR_MULTIPLIER, pmove->origin);
	}
	else
	{
		// all other modes just track some kind of target, so spectator PVS = target PVS
		int target;
		// no valid target ?
		if ( pmove->iuser2 <= 0)
			return;

		// Find the client this player's targeting
		for (target = 0; target < pmove->numphysent; ++target)
		{
			if (pmove->physents[target].info == pmove->iuser2)
				break;
		}

		if (target == pmove->numphysent)
			return;

		// use targets position as own origin for PVS
		VectorCopy( pmove->physents[target].angles, pmove->angles );
		VectorCopy( pmove->physents[target].origin, pmove->origin );

		// no velocity
		VectorCopy( g_vecZero, pmove->velocity );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Use for ease-in, ease-out style interpolation (accel/decel)
//			Used by ducking code.
// Input  : &inputvalue - 
//			&scale - 
// Output : float
//-----------------------------------------------------------------------------
float PM_SplineFraction(const float &inputvalue, const float &scale)
{
	float value = scale * inputvalue;
	float valueSquared = value * value;
	// Nice little ease-in, ease-out spline-like curve
	return 3 * valueSquared - 2 * valueSquared * value;
}

//-----------------------------------------------------------------------------
// Purpose: PM_FixPlayerCrouchStuck
// Input  : &direction - 
//-----------------------------------------------------------------------------
void PM_FixPlayerCrouchStuck(const int &direction)
{
	int     hitent;
	int i;
	vec3_t test;

	hitent = pmove->PM_TestPlayerPosition ( pmove->origin, NULL );
	if (hitent == -1 )
		return;

	VectorCopy( pmove->origin, test );
	for ( i = 0; i < 36; ++i )
	{
		pmove->origin[2] += direction;
		hitent = pmove->PM_TestPlayerPosition ( pmove->origin, NULL );
		if (hitent == -1 )
			return;
	}

	VectorCopy( test, pmove->origin ); // Failed
}

//-----------------------------------------------------------------------------
// Purpose: PM_UnDuck
//-----------------------------------------------------------------------------
void PM_UnDuck(void)
{
	int i;
	pmtrace_t trace;
	vec3_t newOrigin;

	VectorCopy( pmove->origin, newOrigin );

	if ( pmove->onground != -1 && pmove->flags & FL_DUCKING && pmove->bInDuck == false )// XDM: thx 2 Mazor
	{
		for ( i = 0; i < 3; ++i)
			newOrigin[i] += ( pmove->player_mins[1][i] - pmove->player_mins[0][i] );
	}

	trace = pmove->PM_PlayerTrace( newOrigin, newOrigin, PM_NORMAL, -1 );

	if ( !trace.startsolid )
	{
		pmove->usehull = 0;

		// Oh, no, changing hulls stuck us into something, try unsticking downward first.
		trace = pmove->PM_PlayerTrace( newOrigin, newOrigin, PM_NORMAL, -1  );
		if ( trace.startsolid )
		{
			// See if we are stuck?  If so, stay ducked with the duck hull until we have a clear spot
			//Con_Printf( "unstick got stuck\n" );
			pmove->usehull = 1;
			return;
		}

		pmove->flags &= ~FL_DUCKING;
		pmove->bInDuck  = false;
		pmove->view_ofs[2] = VEC_VIEW;
		pmove->flDuckTime = 0;

		VectorCopy( newOrigin, pmove->origin );

		// Recatagorize position since ducking can change origin
		PM_CatagorizePosition();
	}
}

//-----------------------------------------------------------------------------
// Purpose: PM_Duck
//-----------------------------------------------------------------------------
void PM_Duck(void)
{
	int i;
	float duckFraction;

	int buttonsChanged	= (pmove->oldbuttons ^ pmove->cmd.buttons);	// These buttons have changed this frame
	int nButtonPressed	=  buttonsChanged & pmove->cmd.buttons;		// The changed ones still down are "pressed"

	if (pmove->cmd.buttons & IN_DUCK)
		pmove->oldbuttons |= IN_DUCK;
	else
		pmove->oldbuttons &= ~IN_DUCK;

	// Prevent ducking if the iuser3 variable is set
	if ( pmove->iuser3 || pmove->dead )
	{
		// Try to unduck
		if ( pmove->flags & FL_DUCKING )
			PM_UnDuck();

		return;
	}

	if ( pmove->flags & FL_DUCKING )
	{
		pmove->cmd.forwardmove *= 0.333f;
		pmove->cmd.sidemove    *= 0.333f;
		pmove->cmd.upmove      *= 0.333f;
	}

	if ((pmove->cmd.buttons & IN_DUCK) || (pmove->bInDuck) || (pmove->flags & FL_DUCKING))
	{
		if (pmove->cmd.buttons & IN_DUCK)
		{
			if ((nButtonPressed & IN_DUCK) && !(pmove->flags & FL_DUCKING))
			{
				// Use 1 second so super long jump will work
				pmove->flDuckTime = 1000;
				pmove->bInDuck    = true;
			}

//			time = max(0.0f, (1.0f - (float)pmove->flDuckTime / 1000.0f));// XDM3036: __max

			if ( pmove->bInDuck )
			{
				// Finish ducking immediately if duck time is over or not on ground
				if (((float)pmove->flDuckTime / 1000.0f <= ( 1.0f - TIME_TO_DUCK ) ) || ( pmove->onground == -1))
				{
					pmove->usehull = 1;
					pmove->view_ofs[2] = DUCK_VIEW;
					pmove->flags |= FL_DUCKING;
					pmove->bInDuck = false;

					// HACKHACK - Fudge for collision bug - no time to fix this properly
					if ( pmove->onground != -1 )
					{
						for (i = 0; i < 3; ++i)
							pmove->origin[i] -= ( pmove->player_mins[1][i] - pmove->player_mins[0][i] );

						// See if we are stuck?
						PM_FixPlayerCrouchStuck( STUCK_MOVEUP );
						// Recatagorize position since ducking can change origin
						PM_CatagorizePosition();
					}
				}
				else
				{
					float fMore = (DUCK_HULL_MIN - HULL_MIN);
					float fTime = max(0.0f, (1.0f - (float)pmove->flDuckTime / 1000.0f));// XDM3036: __max
					// Calc parametric time
					duckFraction = PM_SplineFraction(fTime, (1.0f/TIME_TO_DUCK));
					pmove->view_ofs[2] = ((DUCK_VIEW - fMore ) * duckFraction) + (VEC_VIEW * (1-duckFraction));
				}
			}
		}
		else// Try to unduck
			PM_UnDuck();
	}
}

//-----------------------------------------------------------------------------
// Purpose: LadderMove
// Input  : *pLadder - 
//-----------------------------------------------------------------------------
void PM_LadderMove(physent_t *pLadder)
{
	vec3_t		ladderCenter;
	trace_t		trace;
	qboolean	onFloor;
	vec3_t		floor;
	vec3_t		modelmins, modelmaxs;

	if ( pmove->movetype == MOVETYPE_NOCLIP )
		return;

	pmove->PM_GetModelBounds( pLadder->model, modelmins, modelmaxs );

	VectorAdd( modelmins, modelmaxs, ladderCenter );
	VectorScale( ladderCenter, 0.5, ladderCenter );

	pmove->movetype = MOVETYPE_FLY;

	// On ladder, convert movement to be relative to the ladder

	VectorCopy( pmove->origin, floor );
	floor[2] += pmove->player_mins[pmove->usehull][2] - 1;

	if ( pmove->PM_PointContents( floor, NULL ) == CONTENTS_SOLID )
		onFloor = true;
	else
		onFloor = false;

	pmove->gravity = 0;
	pmove->PM_TraceModel( pLadder, pmove->origin, ladderCenter, &trace );
	if ( trace.fraction != 1.0 )
	{
		float forward = 0, right = 0;
		vec3_t vpn, v_right;

		AngleVectors( pmove->angles, vpn, v_right, NULL );
		if ( pmove->cmd.buttons & IN_BACK )
			forward -= MAX_CLIMB_SPEED;
		if ( pmove->cmd.buttons & IN_FORWARD )
			forward += MAX_CLIMB_SPEED;
		if ( pmove->cmd.buttons & IN_MOVELEFT )
			right -= MAX_CLIMB_SPEED;
		if ( pmove->cmd.buttons & IN_MOVERIGHT )
			right += MAX_CLIMB_SPEED;

		if ( pmove->cmd.buttons & IN_JUMP )
		{
			pmove->movetype = MOVETYPE_WALK;
			VectorScale( trace.plane.normal, 270, pmove->velocity );
		}
		else
		{
			if ( forward != 0 || right != 0 )
			{
				vec3_t velocity, perp, cross, lateral, tmp;
				float normal;

				//ALERT(at_console, "pev %.2f %.2f %.2f - ",
				//	pev->velocity.x, pev->velocity.y, pev->velocity.z);
				// Calculate player's intended velocity
				//Vector velocity = (forward * gpGlobals->v_forward) + (right * gpGlobals->v_right);
				VectorScale( vpn, forward, velocity );
				VectorMA( velocity, right, v_right, velocity );

				// Perpendicular in the ladder plane
	//					Vector perp = CrossProduct( Vector(0,0,1), trace.vecPlaneNormal );
	//					perp = perp.Normalize();
				VectorClear( tmp );
				tmp[2] = 1;
				CrossProduct( tmp, trace.plane.normal, perp );
				VectorNormalize( perp );

				// decompose velocity into ladder plane
				normal = DotProduct( velocity, trace.plane.normal );
				// This is the velocity into the face of the ladder
				VectorScale( trace.plane.normal, normal, cross );

				// This is the player's additional velocity
				VectorSubtract( velocity, cross, lateral );

				// This turns the velocity into the face of the ladder into velocity that
				// is roughly vertically perpendicular to the face of the ladder.
				// NOTE: It IS possible to face up and move down or face down and move up
				// because the velocity is a sum of the directional velocity and the converted
				// velocity through the face of the ladder -- by design.
				CrossProduct( trace.plane.normal, perp, tmp );
				VectorMA( lateral, -normal, tmp, pmove->velocity );
				if ( onFloor && normal > 0 )	// On ground moving away from the ladder
				{
					VectorMA( pmove->velocity, MAX_CLIMB_SPEED, trace.plane.normal, pmove->velocity );
				}
				//pev->velocity = lateral - (CrossProduct( trace.vecPlaneNormal, perp ) * normal);

//				pmove->Con_Printf("PM_LadderMove() %d %d\n", pmove->flTimeStepSound, pmove->iStepLeft);
/* somehow it doesn't work well
				if (pmove->flTimeStepSound <= 0)// XDM3035a: use sound delay, it suits perfectly
				{
					pmove->Con_Printf("ladder punch! %d\n", pmove->iStepLeft);
//					pmove->punchangle[PITCH] = 14.0f;
					pmove->punchangle[YAW] = 8.0f;

					if (pmove->iStepLeft == 0)
						pmove->punchangle[YAW] *= -1.0f;
				}*/
			}
			else
			{
				VectorClear( pmove->velocity );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Search for ladder
// Output : physent_t
//-----------------------------------------------------------------------------
physent_t *PM_Ladder(void)
{
	int			i;
	physent_t	*pe;
	hull_t		*hull;
	int			num;
	vec3_t		test;

	for (i = 0; i < pmove->nummoveent; ++i)
	{
		pe = &pmove->moveents[i];

		if ( pe->model && (modtype_t)pmove->PM_GetModelType( pe->model ) == mod_brush && pe->skin == CONTENTS_LADDER )
		{

			hull = (hull_t *)pmove->PM_HullForBsp( pe, test );
			num = hull->firstclipnode;

			// Offset the test point appropriately for this hull.
			VectorSubtract ( pmove->origin, test, test);

			// Test the player's hull for intersection with this model
			if ( pmove->PM_HullPointContents (hull, num, test) == CONTENTS_EMPTY)
				continue;

			return pe;
		}
	}
	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: WaterJump
//-----------------------------------------------------------------------------
void PM_WaterJump(void)
{
	if (pmove->waterjumptime > 10000)
		pmove->waterjumptime = 10000;

	if (pmove->waterjumptime == 0)
		return;

	pmove->waterjumptime -= pmove->cmd.msec;

	if (pmove->waterjumptime < 0 || pmove->waterlevel == 0)
	{
		pmove->waterjumptime = 0;
		pmove->flags &= ~FL_WATERJUMP;
	}

	pmove->velocity[0] = pmove->movedir[0];
	pmove->velocity[1] = pmove->movedir[1];
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : void PM_AddGravity
//-----------------------------------------------------------------------------
void PM_AddGravity(void)
{
	float	ent_gravity;

	if (pmove->gravity)
		ent_gravity = pmove->gravity;
	else
		ent_gravity = 1.0;

	// Add gravity incorrectly
	pmove->velocity[2] -= (ent_gravity * pmove->movevars->gravity * pmove->frametime);
	pmove->velocity[2] += pmove->basevelocity[2] * pmove->frametime;
	pmove->basevelocity[2] = 0;
	PM_CheckVelocity();
}

//-----------------------------------------------------------------------------
// Purpose: Does not change the entities velocity at all
// Input  : &push - 
// Output : pmtrace_t
//-----------------------------------------------------------------------------
pmtrace_t PM_PushEntity(const Vector &push)
{
	pmtrace_t	trace;
	vec3_t	end;

	VectorAdd (pmove->origin, push, end);
	trace = pmove->PM_PlayerTrace (pmove->origin, end, PM_NORMAL, -1 );
	VectorCopy (trace.endpos, pmove->origin);

	// So we can run impact function afterwards.
	if (trace.fraction < 1.0 && !trace.allsolid)
		PM_AddToTouched(trace, pmove->velocity);

	return trace;
}

//-----------------------------------------------------------------------------
// Purpose: Dead player flying through air., e.g.
//-----------------------------------------------------------------------------
void PM_Physics_Toss(void)
{
	pmtrace_t trace;
	vec3_t	move;
	float	backoff;

	PM_CheckWater();

	if (pmove->velocity[2] > 0)
		pmove->onground = -1;

	// If on ground and not moving, return.
	if ( pmove->onground != -1 )
	{
//		if (VectorCompare(pmove->basevelocity, g_vecZero) &&
//		    VectorCompare(pmove->velocity, g_vecZero))
		if (pmove->basevelocity.IsZero() && pmove->velocity.IsZero())
			return;
	}

	PM_CheckVelocity ();

// add gravity
	if (pmove->movetype != MOVETYPE_FLY &&
		pmove->movetype != MOVETYPE_BOUNCEMISSILE &&
		pmove->movetype != MOVETYPE_FLYMISSILE)
		PM_AddGravity ();

// move origin
	// Base velocity is not properly accounted for since this entity will move again after the bounce without
	// taking it into account
	VectorAdd (pmove->velocity, pmove->basevelocity, pmove->velocity);

	PM_CheckVelocity();
	VectorScale (pmove->velocity, pmove->frametime, move);
	VectorSubtract (pmove->velocity, pmove->basevelocity, pmove->velocity);

	trace = PM_PushEntity (move);	// Should this clear basevelocity

	PM_CheckVelocity();

	if (trace.allsolid)
	{
		// entity is trapped in another solid
		pmove->onground = trace.ent;
		VectorCopy (g_vecZero, pmove->velocity);
		return;
	}

	if (trace.fraction == 1)
	{
		PM_CheckWater();
		return;
	}

	if (pmove->movetype == MOVETYPE_BOUNCE)
		backoff = 2.0f - pmove->friction;
	else if (pmove->movetype == MOVETYPE_BOUNCEMISSILE)
		backoff = 2.0f;
	else
		backoff = 1.0f;

	PM_ClipVelocity (pmove->velocity, trace.plane.normal, pmove->velocity, backoff);

	// stop if on ground
	if (trace.plane.normal[2] > 0.7f)
	{
		float vel;
		vec3_t base;

		VectorClear( base );
		if (pmove->velocity[2] < pmove->movevars->gravity * pmove->frametime)
		{
			// we're rolling on the ground, add static friction.
			pmove->onground = trace.ent;
			pmove->velocity[2] = 0;
		}

		vel = DotProduct( pmove->velocity, pmove->velocity );

		// Con_DPrintf("%f %f: %.0f %.0f %.0f\n", vel, trace.fraction, ent->velocity[0], ent->velocity[1], ent->velocity[2] );

		if (vel < (30 * 30) || (pmove->movetype != MOVETYPE_BOUNCE && pmove->movetype != MOVETYPE_BOUNCEMISSILE))
		{
			pmove->onground = trace.ent;
			VectorCopy (g_vecZero, pmove->velocity);
		}
		else
		{
			VectorScale (pmove->velocity, (1.0f - trace.fraction) * pmove->frametime * 0.9f, move);
			trace = PM_PushEntity (move);
		}
		VectorSubtract( pmove->velocity, base, pmove->velocity );
	}

// check for in water
	PM_CheckWater();
}

//-----------------------------------------------------------------------------
// Purpose: NoClip
//-----------------------------------------------------------------------------
void PM_NoClip(void)
{
	vec3_t wishvel;
	VectorNormalize(pmove->forward);
	VectorNormalize(pmove->right);
	wishvel = pmove->forward*pmove->cmd.forwardmove + pmove->right*pmove->cmd.sidemove;
	wishvel[2] += pmove->cmd.upmove;// Not *pmove->up, so +moveup command will always drag absolutely upwards
	VectorMA(pmove->origin, pmove->frametime, wishvel, pmove->origin);
	// Zero out the velocity so that we don't accumulate a huge downward velocity from gravity, etc.
	VectorClear(pmove->velocity);

}

// Only allow bunny jumping up to 1.7x server / player maxspeed setting
#define BUNNYJUMP_MAX_SPEED_FACTOR 1.7f

//-----------------------------------------------------------------------------
// Purpose: Corrects bunny jumping ( where player initiates a bunny jump before other
//  movement logic runs, thus making onground == -1 thus making PM_Friction get skipped and
//  running PM_AirMove, which doesn't crop velocity to maxspeed like the ground / other
//  movement logic does.
//-----------------------------------------------------------------------------
void PM_PreventMegaBunnyJumping( void )
{
	// Current player speed
	float spd;
	// If we have to crop, apply this cropping fraction to velocity
	float fraction;
	// Speed at which bunny jumping is limited
	float maxscaledspeed = BUNNYJUMP_MAX_SPEED_FACTOR * pmove->maxspeed;

	// Don't divide by zero
	if ( maxscaledspeed <= 0.0f )
		return;

	spd = Length( pmove->velocity );

	if ( spd <= maxscaledspeed )
		return;

	fraction = ( maxscaledspeed / spd ) * 0.65f; //Returns the modifier for the velocity

	VectorScale( pmove->velocity, fraction, pmove->velocity ); //Crop it down!.
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void PM_Jump(void)
{
	if (pmove->dead)
	{
		pmove->oldbuttons |= IN_JUMP;	// don't jump again until released
		return;
	}

	// See if we are waterjumping.  If so, decrement count and return.
	if ( pmove->waterjumptime )
	{
		pmove->waterjumptime -= pmove->cmd.msec;
		if (pmove->waterjumptime < 0)
			pmove->waterjumptime = 0;

		return;
	}

	// If we are in the water most of the way...
	if (pmove->waterlevel >= 2)
	{	// swimming, not jumping
		pmove->onground = -1;

		if (pmove->watertype == CONTENTS_WATER)    // We move up a certain amount
			pmove->velocity[2] = 100;
		else if (pmove->watertype == CONTENTS_SLIME)
			pmove->velocity[2] = 80;
		else  // LAVA
			pmove->velocity[2] = 50;

		// play swiming sound
		if ( pmove->flSwimTime <= 0 )
		{
			// Don't play sound again for 1 second
			pmove->flSwimTime = 1000;
			pmove->PM_PlaySound(CHAN_BODY, gStepSounds[STEP_WADE][pmove->RandomLong(0,3)], VOL_NORM, ATTN_NORM, 0, pmove->RandomLong(95,105));// XDM3035a
		}
		return;
	}

	// No more effect
 	if ( pmove->onground == -1 )
	{
		// Flag that we jumped.
		// HACK HACK HACK
		// Remove this when the game .dll no longer does physics code!!!!
		pmove->oldbuttons |= IN_JUMP; // don't jump again until released
		return;		// in air, so no effect
	}

	if ( pmove->oldbuttons & IN_JUMP )
		return;		// don't pogo stick

	switch(pmove->RandomLong(0,1))// XDM : jump snd
	{
	case 0: pmove->PM_PlaySound(CHAN_STATIC, "player/pl_jump1.wav", pmove->bInDuck?0.5f:0.75f, pmove->bInDuck?ATTN_IDLE:ATTN_NORM, 0, pmove->RandomLong(96,104)); break;
	case 1: pmove->PM_PlaySound(CHAN_STATIC, "player/pl_jump2.wav", pmove->bInDuck?0.5f:0.75f, pmove->bInDuck?ATTN_IDLE:ATTN_NORM, 0, pmove->RandomLong(96,104)); break;
	}

	// In the air now.
    pmove->onground = -1;
	PM_PreventMegaBunnyJumping();
	PM_PlayStepSound(MapTextureTypeStepType(pmove->chtexturetype), VOL_NORM);

	// See if user can super long jump?
	bool cansuperjump = atoi(pmove->PM_Info_ValueForKey(pmove->physinfo, PHYSKEY_LONGJUMP)) == 1 ? true : false;
	bool cansuperrun = atoi(pmove->PM_Info_ValueForKey(pmove->physinfo, PHYSKEY_HASTE)) == 1?true:false;

	// Adjust for super long jump module
	// UNDONE -- note this should be based on forward angles, not current velocity.
	if (cansuperrun || cansuperjump && Length(pmove->velocity) > 50)
	{
		pmove->punchangle[0] = -5;

		for (int i =0; i < 2; ++i)
			pmove->velocity[i] = pmove->forward[i] * PLAYER_LONGJUMP_SPEED * 1.6f;

		if (cansuperrun)
			pmove->velocity[2] += sqrtf(2 * 800 * 70);
		else
			pmove->velocity[2] += sqrtf(2 * 800 * 56);
	}
	else
	{
		pmove->velocity[2] += sqrtf(2 * 800 * 45);
	}

	// Decay it for simulation
	PM_FixupGravityVelocity();

	// Flag that we jumped.
	pmove->oldbuttons |= IN_JUMP;	// don't jump again until released
}

#define WJ_HEIGHT 8

//-----------------------------------------------------------------------------
// Purpose: PM_CheckWaterJump
//-----------------------------------------------------------------------------
void PM_CheckWaterJump(void)
{
	vec3_t	vecStart, vecEnd;
	vec3_t	flatforward;
	vec3_t	flatvelocity;
	float curspeed;
	pmtrace_t tr;
	int		savehull;

	// Already water jumping.
	if (pmove->waterjumptime)
		return;

	// Don't hop out if we just jumped in
	if (pmove->velocity[2] < -180)
		return; // only hop out if we are moving up

	// See if we are backing up
	flatvelocity[0] = pmove->velocity[0];
	flatvelocity[1] = pmove->velocity[1];
	flatvelocity[2] = 0;

	// Must be moving
	curspeed = VectorNormalize( flatvelocity );

	// see if near an edge
	flatforward[0] = pmove->forward[0];
	flatforward[1] = pmove->forward[1];
	flatforward[2] = 0;
	VectorNormalize (flatforward);

	// Are we backing into water from steps or something?  If so, don't pop forward
	if (curspeed != 0.0f && (DotProduct(flatvelocity, flatforward) < 0.0f))
		return;

	VectorCopy( pmove->origin, vecStart );
	vecStart[2] += WJ_HEIGHT;

	VectorMA ( vecStart, 24, flatforward, vecEnd );
//	pmove->PM_PlaySound( CHAN_VOICE, "common/watersplash.wav", 1, ATTN_NORM, 0, PITCH_NORM );// XDM
	// Trace, this trace should use the point sized collision hull
	savehull = pmove->usehull;
	pmove->usehull = 2;
	tr = pmove->PM_PlayerTrace( vecStart, vecEnd, PM_NORMAL, -1 );
	if ( tr.fraction < 1.0 && fabs( tr.plane.normal[2] ) < 0.1f )  // Facing a near vertical wall?
	{
		vecStart[2] += pmove->player_maxs[ savehull ][2] - WJ_HEIGHT;
		VectorMA( vecStart, 24, flatforward, vecEnd );
		VectorMA( g_vecZero, -50, tr.plane.normal, pmove->movedir );

		tr = pmove->PM_PlayerTrace( vecStart, vecEnd, PM_NORMAL, -1 );
		if ( tr.fraction == 1.0 )
		{
			pmove->waterjumptime = 2000;
			pmove->velocity[2] = 225;
			pmove->oldbuttons |= IN_JUMP;
			pmove->flags |= FL_WATERJUMP;
		}
	}

	// Reset the collision hull
	pmove->usehull = savehull;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void PM_CheckFalling(void)
{
/*	int flags;
#ifdef CLIENT_DLL
	event_args_t eargs;
#endif
*/
	if (pmove->flFallVelocity <= 0.0f)
		return;

	if (pmove->onground != -1 || g_onladder || pmove->waterlevel >= 2)
	{
//		if (/*&& !pmove->dead*/ && pmove->flFallVelocity >= PLAYER_FALL_PUNCH_THRESHHOLD)// XDM3035a: players may land on ladder
		float fvol = 0.0f;// XDM3035: play step sound with this volume

		// land sounds
		if (!pmove->dead)
		{
			if (pmove->flFallVelocity > PLAYER_MAX_SAFE_FALL_SPEED)
			{
				if (pmove->waterlevel > 0)// XDM: most painful sound or big water splash
					pmove->PM_PlaySound(CHAN_STATIC, "common/watersplash.wav", 1.0f, ATTN_NORM, 0, PITCH_NORM);
				else
					switch (pmove->RandomLong(0,2))
					{
					case 0:
						pmove->PM_PlaySound(CHAN_STATIC, "player/pl_fallpain1.wav", 1.0f, ATTN_NORM, 0, PITCH_NORM);
					break;

					case 1:
						pmove->PM_PlaySound(CHAN_STATIC, "player/pl_fallpain2.wav", 1.0f, ATTN_NORM, 0, PITCH_NORM);
					break;

					case 2:
						pmove->PM_PlaySound(CHAN_STATIC, "player/pl_fallpain3.wav", 1.0f, ATTN_NORM, 0, PITCH_NORM);
					break;
				}
			}
			else if (pmove->flFallVelocity > PLAYER_MIN_BOUNCE_SPEED)// * 0.3f
			{
				if (pmove->waterlevel > 0)
					pmove->PM_PlaySound(CHAN_STATIC, "player/pl_wade3.wav", 0.75f, ATTN_NORM, 0, pmove->RandomLong(96,104));
				else if (!g_onladder)// this makes things ugly
					fvol = 1.0f;
					pmove->PM_PlaySound(CHAN_STATIC, "player/pl_jumpland2.wav", 0.75f, ATTN_NORM, 0, pmove->RandomLong(96,104));
			}
		}

		// punch effect
		if (pmove->flFallVelocity > PLAYER_FALL_PUNCH_THRESHHOLD)
		{
			// Knock the screen around a little bit, temporary effect
			pmove->punchangle[0] = 6.0f;
			pmove->punchangle[2] = pmove->flFallVelocity * 0.02f;	// punch z axis

			if (pmove->punchangle[2] > 8)
				pmove->punchangle[2] = 8;

//			pmove->Con_Printf("PM_CheckFalling() left: %d\n", pmove->iStepLeft);
			if (pmove->iStepLeft > 0)// XDM3035a
				pmove->punchangle[2] *= -1;
		}

		// water splash, dust, etc.
		if (pmove->flFallVelocity > PLAYER_MAX_SAFE_FALL_SPEED * 0.5f)// don't play everytime!
		{
#ifdef CLIENT_DLL// if this is a client DLL code, play event locally only (so server won't have to send to me too)
/*			flags = FEV_HOSTONLY | FEV_CLIENT;
			// HACK! I'd rather do this stuff than eat more bandwidth!
			eargs.flags = flags;
			eargs.entindex = pmove->player_index;
			VectorCopy(pmove->origin, eargs.origin);
			VectorCopy(pmove->angles, eargs.angles);
			VectorCopy(pmove->velocity, eargs.velocity);
			eargs.ducking = (pmove->flags & FL_DUCKING);
			eargs.fparam1 = pmove->flFallVelocity;
			eargs.fparam2 = fvol;
			eargs.iparam1 = pmove->waterlevel;
			eargs.iparam2 = 0;
			eargs.bparam1 = (pmove->onground == -1)?0:1;
			eargs.bparam2 = g_onladder;
			EV_PM_Fall(&eargs);// what a fail! we cannot get event index on client side!!
*/
//			CL_PlaybackEventDirect(FEV_HOSTONLY|FEV_CLIENT, pmove->player_index, EV_PM_Fall, (pmove->flags & FL_DUCKING), pmove->origin, pmove->angles, pmove->flFallVelocity, fvol, pmove->waterlevel, 0, (pmove->onground == -1)?0:1, g_onladder);
#else
//wtf? This flag does the opposite!!			flags = FEV_NOTHOST;
			// BUGBUG: how do I sent event from server to everyone else in PVS except me?!
			pmove->PM_PlaybackEventFull(FEV_UPDATE|FEV_SERVER, pmove->player_index, g_usPM_Fall, 0.0f, pmove->origin, pmove->angles, pmove->flFallVelocity, fvol, pmove->waterlevel, 0, (pmove->onground == -1)?0:1, g_onladder);
#endif
		}

		if (fvol > 0.0f)
		{
			// Play landing step right away
			pmove->flTimeStepSound = 0;
			PM_UpdateStepSound();
			// play step sound for current texture
			if (pmove->dead)
				pmove->PM_PlaySound(CHAN_BODY, gSoundsDropBody[pmove->RandomLong(0,NUM_BODYDROP_SOUNDS-1)], fvol, ATTN_NORM, 0, PITCH_NORM);// XDM3037
			else
				PM_PlayStepSound(MapTextureTypeStepType(pmove->chtexturetype), fvol);
		}
//	}
		//	if (pmove->onground != -1 || pmove->waterlevel >= 2)// XDM3035a: clear fall velocity after entering deep water so this water landing won't be played when the player actually steps on the ground
		pmove->flFallVelocity = 0.0f;// reset so hit won't get called twice
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *angles - 
//			*velocity - 
//			&rollangle - 
//			&rollspeed - 
// Output : float
//-----------------------------------------------------------------------------
float PM_CalcRoll(const vec3_t &angles, const vec3_t &velocity, const float &rollangle, const float &rollspeed)
{
    float   sign;
    float   side;
    float   value;
	vec3_t  forward, right, up;

	AngleVectors (angles, forward, right, up);

	side = DotProduct (velocity, right);
	sign = side < 0 ? -1.0f : 1.0f;
	side = fabsf(side);
	value = rollangle;

	if (side < rollspeed)
	{
		side = side * value / rollspeed;
	}
    else
	{
		side = value;
	}

	return side * sign;
}

//-----------------------------------------------------------------------------
// Purpose: Slowly decrease punchangle
// Input  : &punchangle - 
//-----------------------------------------------------------------------------
void PM_DropPunchAngle(Vector &punchangle)
{
	float len = VectorNormalize(punchangle);
	len -= (10.0f + len * 0.5f) * pmove->frametime;
	len = max(len, 0.0f);// XDM3036: __max
	VectorScale(punchangle, len, punchangle);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void PM_CheckParamters( void )
{
	float spd;
	float maxspeed;
	vec3_t	v_angle;

	spd = ( pmove->cmd.forwardmove * pmove->cmd.forwardmove ) +
		  ( pmove->cmd.sidemove * pmove->cmd.sidemove ) +
		  ( pmove->cmd.upmove * pmove->cmd.upmove );
	spd = sqrtf( spd );

	maxspeed = pmove->clientmaxspeed; //atof( pmove->PM_Info_ValueForKey( pmove->physinfo, "maxspd" ) );
	if ( maxspeed != 0.0f )
	{
		pmove->maxspeed = min(maxspeed, pmove->maxspeed);// XDM3036: __min
	}

	if ((spd != 0.0f) && (spd > pmove->maxspeed))
	{
		float fRatio = pmove->maxspeed / spd;
		pmove->cmd.forwardmove *= fRatio;
		pmove->cmd.sidemove    *= fRatio;
		pmove->cmd.upmove      *= fRatio;
	}

	if ( pmove->flags & FL_FROZEN && !( pmove->flags & FL_ONGROUND ))
		pmove->cmd.msec = 25;

	if ( pmove->flags & FL_FROZEN || pmove->flags & FL_ONTRAIN || pmove->dead )
	{
		pmove->cmd.forwardmove = 0;
		pmove->cmd.sidemove    = 0;
		pmove->cmd.upmove      = 0;
//		pmove->cmd.buttons     = 0;
	}

	PM_DropPunchAngle( pmove->punchangle );

	// Take angles from command.
	if ( !pmove->dead )
	{
		VectorCopy ( pmove->cmd.viewangles, v_angle );
		VectorAdd( v_angle, pmove->punchangle, v_angle );

		// Set up view angles.
		pmove->angles[ROLL]	=	PM_CalcRoll ( v_angle, pmove->velocity, pmove->movevars->rollangle, pmove->movevars->rollspeed )*4;
		pmove->angles[PITCH] =	v_angle[PITCH];
		pmove->angles[YAW]   =	v_angle[YAW];
	}
	else
	{
		VectorCopy( pmove->oldangles, pmove->angles );
	}

	// Set dead player view_offset
	if ( pmove->dead )
	{
		pmove->view_ofs[2] = PM_DEAD_VIEWHEIGHT;
	}

	// Adjust client view angles to match values used on server.
	if (pmove->angles[YAW] > 180.0f)
		pmove->angles[YAW] -= 360.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void PM_ReduceTimers(void)
{
	if ( pmove->flTimeStepSound > 0 )
	{
		pmove->flTimeStepSound -= pmove->cmd.msec;
		if ( pmove->flTimeStepSound < 0 )
		{
			pmove->flTimeStepSound = 0;
		}
	}
	if ( pmove->flDuckTime > 0 )
	{
		pmove->flDuckTime -= pmove->cmd.msec;
		if ( pmove->flDuckTime < 0 )
		{
			pmove->flDuckTime = 0;
		}
	}
	if ( pmove->flSwimTime > 0 )
	{
		pmove->flSwimTime -= pmove->cmd.msec;
		if ( pmove->flSwimTime < 0 )
		{
			pmove->flSwimTime = 0;
		}
	}
}

/*
=============
PlayerMove

Returns with origin, angles, and velocity modified in place.

Numtouch and touchindex[] will be set if any of the physents
were contacted during the move.
=============
*/
void PM_PlayerMove(qboolean server)
{
	physent_t *pLadder = NULL;

	// Are we running server code?
	pmove->server = server;

	// Adjust speeds etc.
	PM_CheckParamters();

	// Assume we don't touch anything
	pmove->numtouch = 0;

	// # of msec to apply movement
	pmove->frametime = (float)pmove->cmd.msec * 0.001f;// XDM3035: 20100515

	PM_ReduceTimers();

	// Convert view angles to vectors
	AngleVectors (pmove->angles, pmove->forward, pmove->right, pmove->up);

	// PM_ShowClipBox();

	// Special handling for spectator and observers. (iuser1 is set if the player's in observer mode)
	if ( pmove->spectator || pmove->iuser1 > 0 )
	{
		PM_SpectatorMove();
		PM_CatagorizePosition();
		return;
	}

	// Always try and unstick us unless we are in NOCLIP mode
	if ( pmove->movetype != MOVETYPE_NOCLIP && pmove->movetype != MOVETYPE_NONE )
	{
		if ( PM_CheckStuck() )
		{
			return;  // Can't move, we're stuck
		}
	}

	// Now that we are "unstuck", see where we are ( waterlevel and type, pmove->onground ).
	PM_CatagorizePosition();

	// Store off the starting water level
	pmove->oldwaterlevel = pmove->waterlevel;

	// If we are not on ground, store off how fast we are moving down
	if ( pmove->onground == -1 )
	{
		pmove->flFallVelocity = -pmove->velocity[2];
	}

	g_onladder = 0;
	// Don't run ladder code if dead or on a train
	if ( !pmove->dead && !(pmove->flags & FL_ONTRAIN) )
	{
		if (atoi(pmove->PM_Info_ValueForKey(pmove->physinfo, PHYSKEY_IGNORELADDER)) <= 0)// XDM3037: ignore ladder
			pLadder = PM_Ladder();

		if ( pLadder )
			g_onladder = 1;
	}

//	PM_UpdateStepSound();
	PM_Duck();

	// Don't run ladder code if dead or on a train
	if ( !pmove->dead && !(pmove->flags & FL_ONTRAIN) )
	{
		if ( pLadder )
		{
			PM_LadderMove( pLadder );
			PM_CheckFalling();// XDM3035a: :p
		}
		else if ( pmove->movetype != MOVETYPE_WALK &&
			      pmove->movetype != MOVETYPE_NOCLIP )
		{
			// Clear ladder stuff unless player is noclipping
			//  it will be set immediately again next frame if necessary
			pmove->movetype = MOVETYPE_WALK;
		}
	}

//	pmove->Con_Printf("PM_PlayerMove(%d) flFallVelocity: %f\n", server, pmove->flFallVelocity);

	// XDM3035a TESTME:!! Moved here so flTimeStepSound == 0 can be catched by PM_LadderMove
	PM_UpdateStepSound();

	// XDM3035a UNDONE: this causes players to suddenly stop when hitting +use
	// Slow down, I'm pulling it! (a box maybe) but only when I'm standing on ground
	if ((pmove->onground != -1) && (pmove->cmd.buttons & IN_USE))
	{
		VectorScale( pmove->velocity, 0.3f, pmove->velocity );
	}

	// Handle movement
	switch ( pmove->movetype )
	{
	default:
		pmove->Con_DPrintf("Bogus pmove player movetype %i on (%i) 0=cl 1=sv\n", pmove->movetype, pmove->server);
		break;

	case MOVETYPE_NONE:
		break;

	case MOVETYPE_NOCLIP:
		PM_NoClip();
		break;

	case MOVETYPE_TOSS:
	case MOVETYPE_BOUNCE:
		PM_Physics_Toss();
		break;

	case MOVETYPE_FLY:
		PM_CheckWater();

		// Was jump button pressed?
		// If so, set velocity to 270 away from ladder.  This is currently wrong.
		// Also, set MOVE_TYPE to walk, too.
		if (pmove->cmd.buttons & IN_JUMP)
		{
			if (!pLadder)
				PM_Jump();
		}
		else
			pmove->oldbuttons &= ~IN_JUMP;

		// Perform the move accounting for any base velocity.
		VectorAdd(pmove->velocity, pmove->basevelocity, pmove->velocity);
		PM_FlyMove();
		VectorSubtract(pmove->velocity, pmove->basevelocity, pmove->velocity);
		break;

	case MOVETYPE_WALK:
		if (!PM_InWater())
			PM_AddCorrectGravity();

		// If we are leaping out of the water, just update the counters.
		if ( pmove->waterjumptime )
		{
			PM_WaterJump();
			PM_FlyMove();

			// Make sure waterlevel is set correctly
			PM_CheckWater();
			return;
		}

		// If we are swimming in the water, see if we are nudging against a place we can jump up out
		//  of, and, if so, start out jump.  Otherwise, if we are not moving up, then reset jump timer to 0
		if ( pmove->waterlevel >= 2 )
		{
			if ( pmove->waterlevel == 2 )
			{
				PM_CheckWaterJump();
			}

			// If we are falling again, then we must not trying to jump out of water any more.
			if ( pmove->velocity[2] < 0 && pmove->waterjumptime )
			{
				pmove->waterjumptime = 0;
			}

			// Was jump button pressed?
			if (pmove->cmd.buttons & IN_JUMP)
			{
				PM_Jump ();
			}
			else
			{
				pmove->oldbuttons &= ~IN_JUMP;
			}

			// Perform regular water movement
			PM_WaterMove();

			VectorSubtract (pmove->velocity, pmove->basevelocity, pmove->velocity);

			// Get a final position
			PM_CatagorizePosition();
		}
		else// Not underwater
		{
			// Was jump button pressed?
			if (pmove->cmd.buttons & IN_JUMP)
			{
				if (!pLadder)
					PM_Jump ();
			}
			else
			{
				pmove->oldbuttons &= ~IN_JUMP;
			}

			// Fricion is handled before we add in any base velocity. That way, if we are on a conveyor,
			//  we don't slow when standing still, relative to the conveyor.
			if ( pmove->onground != -1 )
			{
				pmove->velocity[2] = 0.0f;
				PM_Friction();
			}

			// Make sure velocity is valid.
			PM_CheckVelocity();

			// Are we on ground now
			if ( pmove->onground != -1 )
			{
				PM_WalkMove();
			}
			else
			{
				PM_AirMove();  // Take into account movement when in air.
			}

			// Set final flags.
			PM_CatagorizePosition();

			// Now pull the base velocity back out.
			// Base velocity is set if you are on a moving object, like
			//  a conveyor (or maybe another monster?)
			VectorSubtract (pmove->velocity, pmove->basevelocity, pmove->velocity );

			// Make sure velocity is valid.
			PM_CheckVelocity();

			// Add any remaining gravitational component.
			if ( !PM_InWater() )
			{
				PM_FixupGravityVelocity();
			}

			// If we are on ground, no downward velocity.
			if (pmove->onground != -1)
			{
				pmove->velocity[2] = 0.0f;
			}

			// See if we landed on the ground with enough force to play
			// a landing sound.
			PM_CheckFalling();
		}

		// Did we enter or leave the water?
		if ((pmove->oldwaterlevel == 0 && pmove->waterlevel != 0) ||
			(pmove->oldwaterlevel != 0 && pmove->waterlevel == 0))
		{
			pmove->PM_PlaySound(CHAN_BODY, gStepSounds[STEP_WADE][pmove->RandomLong(0,3)], VOL_NORM, ATTN_NORM, 0, pmove->RandomLong(95,105));// XDM3035a
		}

//		PM_PlayWaterSounds();
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void PM_CreateStuckTable( void )
{
	float x, y, z;
	int idx;
	int i;
	float zi[3];

	memset(rgv3tStuckTable, 0, 54 * sizeof(vec3_t));

	idx = 0;
	// Little Moves.
	x = y = 0;
	// Z moves
	for (z = -0.125 ; z <= 0.125 ; z += 0.125f)
	{
		rgv3tStuckTable[idx][0] = x;
		rgv3tStuckTable[idx][1] = y;
		rgv3tStuckTable[idx][2] = z;
		++idx;
	}
	x = z = 0;
	// Y moves
	for (y = -0.125 ; y <= 0.125 ; y += 0.125f)
	{
		rgv3tStuckTable[idx][0] = x;
		rgv3tStuckTable[idx][1] = y;
		rgv3tStuckTable[idx][2] = z;
		++idx;
	}
	y = z = 0;
	// X moves
	for (x = -0.125 ; x <= 0.125 ; x += 0.125f)
	{
		rgv3tStuckTable[idx][0] = x;
		rgv3tStuckTable[idx][1] = y;
		rgv3tStuckTable[idx][2] = z;
		++idx;
	}

	// Remaining multi axis nudges.
	for ( x = -0.125; x <= 0.125; x += 0.250f )
	{
		for ( y = -0.125; y <= 0.125; y += 0.250f )
		{
			for ( z = -0.125; z <= 0.125; z += 0.250f )
			{
				rgv3tStuckTable[idx][0] = x;
				rgv3tStuckTable[idx][1] = y;
				rgv3tStuckTable[idx][2] = z;
				++idx;
			}
		}
	}

	// Big Moves.
	x = y = 0;
	zi[0] = 0.0f;
	zi[1] = 1.0f;
	zi[2] = 6.0f;

	for (i = 0; i < 3; ++i)
	{
		// Z moves
		z = zi[i];
		rgv3tStuckTable[idx][0] = x;
		rgv3tStuckTable[idx][1] = y;
		rgv3tStuckTable[idx][2] = z;
		++idx;
	}

	x = z = 0;

	// Y moves
	for (y = -2.0f ; y <= 2.0f ; y += 2.0f)
	{
		rgv3tStuckTable[idx][0] = x;
		rgv3tStuckTable[idx][1] = y;
		rgv3tStuckTable[idx][2] = z;
		++idx;
	}
	y = z = 0;
	// X moves
	for (x = -2.0f ; x <= 2.0f ; x += 2.0f)
	{
		rgv3tStuckTable[idx][0] = x;
		rgv3tStuckTable[idx][1] = y;
		rgv3tStuckTable[idx][2] = z;
		++idx;
	}

	// Remaining multi axis nudges.
	for (i = 0 ; i < 3; ++i)
	{
		z = zi[i];

		for (x = -2.0f ; x <= 2.0f ; x += 2.0f)
		{
			for (y = -2.0f ; y <= 2.0f ; y += 2.0f)
			{
				rgv3tStuckTable[idx][0] = x;
				rgv3tStuckTable[idx][1] = y;
				rgv3tStuckTable[idx][2] = z;
				++idx;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
/*
This module implements the shared player physics code between any particular game and
the engine.  The same PM_Move routine is built into the game .dll and the client .dll and is
invoked by each side as appropriate.  There should be no distinction, internally, between server
and client.  This will ensure that prediction behaves appropriately.
*/
// Input  : *ppmove - 
//			server - 
// Output : void PM_Move
//-----------------------------------------------------------------------------
void PM_Move ( struct playermove_s *ppmove, int server )
{
	ASSERT(pm_shared_initialized);

	pmove = ppmove;

	if (pmove->flags & FL_FROZEN)// XDM3035b
	{
		pmove->cmd.buttons = 0;
		pmove->oldbuttons = 0;
	}

	PM_PlayerMove((server != 0) ? true : false);

	if (pmove->onground != -1)
	{
		pmove->flags |= FL_ONGROUND;
//TEST		pmove->effects &= ~EF_BRIGHTFIELD;// XDM3037
	}
	else
	{
		pmove->flags &= ~FL_ONGROUND;
//TEST		pmove->effects |= EF_BRIGHTFIELD;// XDM3037
	}

	// In single player, reset friction after each movement to FrictionModifier Triggers work still.
	if (/*!pmove->multiplayer && XDM3035b: TESTME! */(pmove->movetype == MOVETYPE_WALK))
	{
		pmove->friction = 1.0f;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : ent - 
// Output : int
//-----------------------------------------------------------------------------
int PM_GetVisEntInfo( int ent )
{
	if ( ent >= 0 && ent <= pmove->numvisent )
	{
		return pmove->visents[ ent ].info;
	}
	return -1;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : ent - 
// Output : int
//-----------------------------------------------------------------------------
int PM_GetPhysEntInfo( int ent )
{
	if ( ent >= 0 && ent <= pmove->numphysent )
	{
		return pmove->physents[ ent ].info;
	}
	return -1;
}

//-----------------------------------------------------------------------------
// Purpose: PM_Init external function
// Input  : *ppmove - 
//-----------------------------------------------------------------------------
void PM_Init( struct playermove_s *ppmove )
{
	ASSERT(pm_shared_initialized == false);

	pmove = ppmove;
#ifdef CLIENT_DLL
	pmove->Con_DPrintf("PM_Init(client %d) (client)\n", ppmove->player_index);
#else
	pmove->Con_DPrintf("PM_Init(client %d) (server)\n", ppmove->player_index);
#endif

	PM_CreateStuckTable();
	PM_InitTextureTypes();

//	g_usPM_Fall = PRECACHE_EVENT(1, "events/pm/fall.sc");

	pm_shared_initialized = 1;
}
