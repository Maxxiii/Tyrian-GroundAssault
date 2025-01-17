/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*
*	This product contains software technology licensed from Id
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
*	All Rights Reserved.
*
*   This source code contains proprietary and confidential information of
*   Valve LLC and its suppliers.  Access to this code is restricted to
*   persons who have executed a written SDK license with Valve.  Any access,
*   use or distribution of this code by or to any unlicensed person is illegal.
*
****/
/*

===== turret.cpp ========================================================

*/

//
// TODO:
//		Take advantage of new monster fields like m_hEnemy and get rid of that OFFSET() stuff
//		Revisit enemy validation stuff, maybe it's not necessary with the newest monster code
//

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "effects.h"
#include "pm_materials.h"
#include "projectiles.h"
#include "msg_fx.h"
//extern Vector VecBModelOrigin( entvars_t* pevBModel );

#define TURRET_GLOW_SPRITE "sprites/anim_spr10.spr"

#define TURRET_SHOTS			2
#define TURRET_RANGE			4096
#define TURRET_SPREAD			g_vecZero
#define TURRET_TURNRATE			30// angles per 0.1 second
#define TURRET_MAXWAIT			15// seconds turret will stay active w/o a target
#define TURRET_MAXSPIN			5// seconds turret barrel will spin w/o a target
#define TURRET_MACHINE_VOLUME	0.8

typedef enum
{
	TURRET_ANIM_NONE = 0,
	TURRET_ANIM_FIRE,
	TURRET_ANIM_SPIN,
	TURRET_ANIM_DEPLOY,
	TURRET_ANIM_RETIRE,
	TURRET_ANIM_DIE,
} TURRET_ANIM;

class CBaseTurret : public CBaseMonster
{
public:
	virtual void Spawn(void);
	virtual void Precache(void);
	virtual void KeyValue(KeyValueData *pkvd);
	virtual void TraceAttack(CBaseEntity *pAttacker, float flDamage, const Vector &vecDir, TraceResult *ptr, int bitsDamageType);
	virtual int TakeDamage(CBaseEntity *pInflictor, CBaseEntity *pAttacker, float flDamage, int bitsDamageType);
	virtual void Killed(CBaseEntity *pInflictor, CBaseEntity *pAttacker, int iGib);// XDM3035a
	virtual int Classify(void);
//	virtual int BloodColor(void) { return DONT_BLEED; }
	virtual bool GibMonster(void) { return false; };
	virtual BOOL ShouldGibMonster(int iGib) { return FALSE; };// XDM3035a
	virtual BOOL HasHumanGibs(void) { return FALSE; };// XDM3035c
	virtual BOOL IsPushable(void) { return FALSE; }// XDM
	virtual int Save(CSave &save);
	virtual int Restore(CRestore &restore);
	void EXPORT TurretUse(CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value);
	// Think functions
	void EXPORT ActiveThink(void);
	void EXPORT SearchThink(void);
	void EXPORT AutoSearchThink(void);
	void EXPORT TurretDeath(void);
	virtual void EXPORT SpinDownCall(void) { m_iSpin = 0; }
	virtual void EXPORT SpinUpCall(void) { m_iSpin = 1; }
	// void SpinDown(void);
	// float EXPORT SpinDownCall(void) { return SpinDown(); }
	// virtual float SpinDown(void) { return 0;}
	// virtual float Retire(void) { return 0;}
	void EXPORT Deploy(void);
	void EXPORT Retire(void);
	void EXPORT Initialize(void);
	virtual void Shoot(Vector &vecSrc, Vector &vecDirToEnemy) { };
	virtual void Ping(void);
	virtual void EyeOn(void);
	virtual void EyeOff(void);
	virtual void AlertSound(void);// XDM
	virtual void PainSound(void);
	virtual void DeathSound(void);
	virtual void SetEyeColor(int r, int g, int b);// XDM
	virtual void SetEyeBrightness(int a);// XDM
	// other functions
	void SetTurretAnim(TURRET_ANIM anim);
	int MoveTurret(void);

	static TYPEDESCRIPTION m_SaveData[];

	float m_flMaxSpin;// Max time to spin the barrel w/o a target
	int m_iSpin;
	CSprite *m_pEyeGlow;
	int	m_eyeBrightness;
	int	m_iDeployHeight;
	int	m_iRetractHeight;
	int m_iMinPitch;
	int m_iBaseTurnRate;// angles per second
	float m_fTurnRate;// actual turn rate
	int m_iOrientation;// 0 = floor, 1 = Ceiling
	int	m_iOn;
	int m_fBeserk;			// Sometimes this bitch will just freak out
	int m_iAutoStart;		// true if the turret auto deploys when a target
					// enters its range
	Vector m_vecLastSight;
	float m_flLastSight;	// Last time we saw a target
	float m_flMaxWait;		// Max time to seach w/o a target
// XDM	int m_iSearchSpeed;		// Not Used!
	// movement
	float	m_flStartYaw;
	Vector	m_vecCurAngles;
	Vector	m_vecGoalAngles;
	float	m_flPingTime;	// Time until the next ping, used when searching
	float	m_flSpinUpTime;	// Amount of time until the barrel should spin down when searching
//	BOOL	m_flInSpinProc;	// We're spinning up/dow now! Don't shoot! XDM.
	int		m_fireMode;
};

TYPEDESCRIPTION	CBaseTurret::m_SaveData[] =
{
	DEFINE_FIELD( CBaseTurret, m_flMaxSpin, FIELD_FLOAT ),
	DEFINE_FIELD( CBaseTurret, m_iSpin, FIELD_INTEGER ),
	DEFINE_FIELD( CBaseTurret, m_pEyeGlow, FIELD_CLASSPTR ),
	DEFINE_FIELD( CBaseTurret, m_eyeBrightness, FIELD_INTEGER ),
	DEFINE_FIELD( CBaseTurret, m_iDeployHeight, FIELD_INTEGER ),
	DEFINE_FIELD( CBaseTurret, m_iRetractHeight, FIELD_INTEGER ),
	DEFINE_FIELD( CBaseTurret, m_iMinPitch, FIELD_INTEGER ),
	DEFINE_FIELD( CBaseTurret, m_iBaseTurnRate, FIELD_INTEGER ),
	DEFINE_FIELD( CBaseTurret, m_fTurnRate, FIELD_FLOAT ),
	DEFINE_FIELD( CBaseTurret, m_iOrientation, FIELD_INTEGER ),
	DEFINE_FIELD( CBaseTurret, m_iOn, FIELD_INTEGER ),
	DEFINE_FIELD( CBaseTurret, m_fBeserk, FIELD_INTEGER ),
	DEFINE_FIELD( CBaseTurret, m_iAutoStart, FIELD_INTEGER ),
	DEFINE_FIELD( CBaseTurret, m_vecLastSight, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( CBaseTurret, m_flLastSight, FIELD_TIME ),
	DEFINE_FIELD( CBaseTurret, m_flMaxWait, FIELD_FLOAT ),
//	DEFINE_FIELD( CBaseTurret, m_iSearchSpeed, FIELD_INTEGER ),
	DEFINE_FIELD( CBaseTurret, m_flStartYaw, FIELD_FLOAT ),
	DEFINE_FIELD( CBaseTurret, m_vecCurAngles, FIELD_VECTOR ),
	DEFINE_FIELD( CBaseTurret, m_vecGoalAngles, FIELD_VECTOR ),
	DEFINE_FIELD( CBaseTurret, m_flPingTime, FIELD_TIME ),
	DEFINE_FIELD( CBaseTurret, m_flSpinUpTime, FIELD_TIME ),
//	DEFINE_FIELD( CBaseTurret, m_flInSpinProc, FIELD_BOOLEAN ),
};

IMPLEMENT_SAVERESTORE( CBaseTurret, CBaseMonster );


void CBaseTurret::KeyValue(KeyValueData *pkvd)
{
	if (FStrEq(pkvd->szKeyName, "maxsleep"))
	{
		m_flMaxWait = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "orientation"))
	{
		m_iOrientation = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;

	}
	else if (FStrEq(pkvd->szKeyName, "searchspeed"))
	{
//		m_iSearchSpeed = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;

	}
	else if (FStrEq(pkvd->szKeyName, "turnrate"))
	{
		m_iBaseTurnRate = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "style") ||
			 FStrEq(pkvd->szKeyName, "height") ||
			 FStrEq(pkvd->szKeyName, "value1") ||
			 FStrEq(pkvd->szKeyName, "value2") ||
			 FStrEq(pkvd->szKeyName, "value3"))
	{
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "firemode"))// XDM
	{
		m_fireMode = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CBaseMonster::KeyValue(pkvd);
}

void CBaseTurret::Spawn(void)
{
	Precache();
	pev->nextthink		= gpGlobals->time + 1;
	pev->movetype		= MOVETYPE_FLY;
	pev->sequence		= 0;
	pev->frame			= 0;
	pev->solid			= SOLID_SLIDEBOX;
	pev->takedamage		= DAMAGE_AIM;
	pev->flags			|= (FL_MONSTER | FL_IMMUNE_SLIME);
	MonsterInit();// Important to call this in right order!
	SetUse(&CBaseTurret::TurretUse);

	if ((pev->spawnflags & SF_MONSTER_TURRET_AUTOACTIVATE) && !(pev->spawnflags & SF_MONSTER_TURRET_STARTINACTIVE))
		m_iAutoStart = TRUE;

	ResetSequenceInfo();
	SetBoneController(0, 0);
	SetBoneController(1, 0);
	m_flFieldOfView = VIEW_FIELD_FULL;
	m_bloodColor = DONT_BLEED;
	// m_flSightRange = TURRET_RANGE;
	m_MonsterState = MONSTERSTATE_NONE;
}

void CBaseTurret::Precache(void)
{
	PRECACHE_SOUND("turret/tu_ping.wav");
	PRECACHE_SOUND("turret/tu_active2.wav");
	PRECACHE_SOUND("turret/tu_die.wav");
	PRECACHE_SOUND("turret/tu_die2.wav");
	PRECACHE_SOUND("turret/tu_die3.wav");
	PRECACHE_SOUND("turret/tu_retract.wav");
	PRECACHE_SOUND("turret/tu_deploy.wav");
	PRECACHE_SOUND("turret/tu_spinup.wav");
	PRECACHE_SOUND("turret/tu_spindown.wav");
	PRECACHE_SOUND("turret/tu_search.wav");
	PRECACHE_SOUND("turret/tu_alert.wav");
	PRECACHE_SOUND("common/null.wav");
}

void CBaseTurret::Initialize(void)
{
	m_iOn = 0;
	m_fBeserk = 0;
	m_iSpin = 0;
	SetBoneController(0, 0);
	SetBoneController(1, 0);
	if (m_iBaseTurnRate == 0) m_iBaseTurnRate = TURRET_TURNRATE;
	if (m_flMaxWait == 0) m_flMaxWait = TURRET_MAXWAIT;
	m_flStartYaw = pev->angles.y;
	if (m_iOrientation == 1)
	{
		pev->idealpitch = 180;
		pev->angles.x = 180;
		pev->view_ofs.z = -pev->view_ofs.z;
		pev->effects |= EF_INVLIGHT;
		pev->angles.y = pev->angles.y + 180;
		if (pev->angles.y > 360)
			pev->angles.y = pev->angles.y - 360;
	}

	m_vecGoalAngles.x = 0;

	if (m_iAutoStart)
	{
		m_flLastSight = gpGlobals->time + m_flMaxWait;
		SetThink(&CBaseTurret::AutoSearchThink);
		pev->nextthink = gpGlobals->time + .1;
	}
	else
		SetThink(&CBaseEntity::SUB_DoNothing);
}

void CBaseTurret::TurretUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if (pActivator->IsPlayer()/* && (pActivator->pev->weapons & (1<<WEAPON_SUIT))*/)// XDM: players can reactivate turrets/* with HEV computer*/
	{
		pev->impulse = 1;
		pev->owner = pActivator->edict();
	}

	if (!ShouldToggle(useType, m_iOn == 1))
		return;

	if (m_iOn)
	{
		m_hEnemy = NULL;
		pev->nextthink = gpGlobals->time + 0.1;
		m_iAutoStart = FALSE;// switching off a turret disables autostart
		//!!!! this should spin down first! !BUGBUG
		SetThink(&CBaseTurret::Retire);
	}
	else
	{
		pev->nextthink = gpGlobals->time + 0.1; // turn on delay

		// if the turret is flagged as an autoactivate turret, re-enable it's ability open self.
		if (pev->spawnflags & SF_MONSTER_TURRET_AUTOACTIVATE)
			m_iAutoStart = TRUE;

		SetThink(&CBaseTurret::Deploy);
	}
}

void CBaseTurret::Ping(void)
{
	// make the pinging noise every second while searching
	if (m_flPingTime == 0)
	{
		m_flPingTime = gpGlobals->time + 1;
	}
	else if (m_flPingTime <= gpGlobals->time)
	{
		m_flPingTime = gpGlobals->time + 1;
		EMIT_SOUND(ENT(pev), CHAN_ITEM, "turret/tu_ping.wav", VOL_NORM, ATTN_NORM);// TURRET_MACHINE_VOLUME?

		SetEyeColor(255,0,0);// XDM: after activation (Deploy()) the glow is still yellow. This line fixes it.
		EyeOn();
	}
	else if (m_eyeBrightness > 0)
	{
		EyeOff();
	}
}

void CBaseTurret::EyeOn(void)
{
	if (m_pEyeGlow)
	{
		if (m_eyeBrightness != 255)
			m_eyeBrightness = 255;

		m_pEyeGlow->SetBrightness( m_eyeBrightness );
	}
}

void CBaseTurret::EyeOff(void)
{
	if (m_pEyeGlow)
	{
		if (m_eyeBrightness > 0)
		{
			m_eyeBrightness = max( 0, m_eyeBrightness - 30 );
			m_pEyeGlow->SetBrightness( m_eyeBrightness );
		}
	}
}

void CBaseTurret::SetEyeColor(int r, int g, int b)
{
	if (m_pEyeGlow)
		m_pEyeGlow->SetColor(r,g,b);
}

void CBaseTurret::SetEyeBrightness(int a)
{
	if (m_pEyeGlow)
		m_pEyeGlow->SetBrightness(a);
}

void CBaseTurret::ActiveThink(void)
{
	int fAttack = 0;
	Vector vecDirToEnemy;

	pev->nextthink = gpGlobals->time + 0.1;
	StudioFrameAdvance();

	if ((!m_iOn) || (m_hEnemy == NULL))
	{
		m_hEnemy = NULL;
		m_flLastSight = gpGlobals->time + m_flMaxWait;
		SetThink(&CBaseTurret::SearchThink);
		return;
	}

	// if it's dead, look for something new
	if (!m_hEnemy->IsAlive())
	{
		if (!m_flLastSight)
		{
			m_flLastSight = gpGlobals->time + 0.5; // continue-shooting timeout
		}
		else
		{
			if (gpGlobals->time > m_flLastSight)
			{
				m_hEnemy = NULL;
				m_flLastSight = gpGlobals->time + m_flMaxWait;
				SetThink(&CBaseTurret::SearchThink);
				return;
			}
		}
	}

	Vector vecMid = pev->origin + pev->view_ofs;
	Vector vecMidEnemy = m_hEnemy->BodyTarget(vecMid);
	// Look for our current enemy
	bool fEnemyVisible = FBoxVisible(m_hEnemy, vecMidEnemy, 0.0f);
	vecDirToEnemy = vecMidEnemy - vecMid;	// calculate dir and dist to enemy
	float flDistToEnemy = vecDirToEnemy.Length();
	Vector vec = UTIL_VecToAngles(vecMidEnemy - vecMid);
	// Current enmey is not visible.
	if (!fEnemyVisible || (flDistToEnemy > TURRET_RANGE))
	{
		if (!m_flLastSight)
			m_flLastSight = gpGlobals->time + 0.5;
		else
		{
			// Should we look for a new target?
			if (gpGlobals->time > m_flLastSight)
			{
				m_hEnemy = NULL;
				m_flLastSight = gpGlobals->time + m_flMaxWait;
				SetThink(&CBaseTurret::SearchThink);
				return;
			}
		}
		fEnemyVisible = false;
	}
	else
		m_vecLastSight = vecMidEnemy;

	UTIL_MakeAimVectors(m_vecCurAngles);
	/*
	ALERT( at_console, "%.0f %.0f : %.2f %.2f %.2f\n",
		m_vecCurAngles.x, m_vecCurAngles.y,
		gpGlobals->v_forward.x, gpGlobals->v_forward.y, gpGlobals->v_forward.z );
	*/
	Vector vecLOS = vecDirToEnemy; //vecMid - m_vecLastSight;
	vecLOS = vecLOS.Normalize();
	// Is the Gun looking at the target
	if (DotProduct(vecLOS, gpGlobals->v_forward) <= 0.866) // 30 degree slop
		fAttack = FALSE;
	else
		fAttack = TRUE;
	// fire the gun

	// XDM
	if (/*!m_flInSpinProc && */m_iSpin && ((fAttack) || (m_fBeserk)))
	{
		Vector vecSrc, vecAng;
		GetAttachment(0, vecSrc, vecAng);
		SetTurretAnim(TURRET_ANIM_FIRE);
		Shoot(vecSrc, gpGlobals->v_forward);
//		UTIL_ShowLine(vecSrc, vecSrc+gpGlobals->v_forward*20, 2.0f, 255, 240, 0);
	}
	else
	{
		SetTurretAnim(TURRET_ANIM_SPIN);
	}
	//move the gun
	if (m_fBeserk)
	{
		if (RANDOM_LONG(0,9) == 0)
		{
			m_vecGoalAngles.y = RANDOM_FLOAT(0,360);
			m_vecGoalAngles.x = RANDOM_FLOAT(0,90) - 90 * m_iOrientation;
			TakeDamage(this, this, 1, DMG_GENERIC); // don't beserk forever
			return;
		}
	}
	else if (fEnemyVisible)
	{
		if (vec.y > 360)
			vec.y -= 360;

		if (vec.y < 0)
			vec.y += 360;

		//ALERT(at_console, "[%.2f]", vec.x);
		if (vec.x < -180)
			vec.x += 360;

		if (vec.x > 180)
			vec.x -= 360;

		// now all numbers should be in [1...360]
		// pin to turret limitations to [-90...15]
		if (m_iOrientation == 0)
		{
			if (vec.x > 90)
				vec.x = 90;
			else if (vec.x < m_iMinPitch)
				vec.x = m_iMinPitch;
		}
		else
		{
			if (vec.x < -90)
				vec.x = -90;
			else if (vec.x > -m_iMinPitch)
				vec.x = -m_iMinPitch;
		}
		// ALERT(at_console, "->[%.2f]\n", vec.x);
		m_vecGoalAngles.y = vec.y;
		m_vecGoalAngles.x = vec.x;
	}
	SpinUpCall();
	MoveTurret();
}

void CBaseTurret::Deploy(void)
{
	pev->nextthink = gpGlobals->time + 0.1;
	StudioFrameAdvance();

	if (pev->sequence != TURRET_ANIM_DEPLOY)
	{
		if(m_pEyeGlow)// XDM: turn on the glow, startup color: yellow
		{
			EyeOn();
			SetEyeColor(255,255,0);
		}
		m_iOn = 1;
		SetTurretAnim(TURRET_ANIM_DEPLOY);
		//EMIT_SOUND(ENT(pev), CHAN_BODY, "turret/tu_deploy.wav", TURRET_MACHINE_VOLUME, ATTN_NORM);
		AlertSound();
		SUB_UseTargets( this, USE_ON, 0 );
	}

	if (m_fSequenceFinished)
	{
		pev->maxs.z = m_iDeployHeight;
		pev->mins.z = -m_iDeployHeight;
		UTIL_SetSize(pev, pev->mins, pev->maxs);

		m_vecCurAngles.x = 0;

		if (m_iOrientation == 1)
		{
			m_vecCurAngles.y = UTIL_AngleMod( pev->angles.y + 180 );
		}
		else
		{
			m_vecCurAngles.y = UTIL_AngleMod( pev->angles.y );
		}

		if(m_pEyeGlow)
			EyeOff();// XDM: activation completed

		SetTurretAnim(TURRET_ANIM_SPIN);
		pev->framerate = 0;
		SetThink(&CBaseTurret::SearchThink);
	}

	m_flLastSight = gpGlobals->time + m_flMaxWait;
}

void CBaseTurret::Retire(void)
{
	// make the turret level
	m_vecGoalAngles.x = 0;
	m_vecGoalAngles.y = m_flStartYaw;
	pev->nextthink = gpGlobals->time + 0.1;
	StudioFrameAdvance();

	EyeOn();
	SetEyeColor(0,255,0);// XDM: suspend color: green!
//	EyeOff();// XDM: Moved down

	if (!MoveTurret())
	{
		if (m_iSpin)
		{
			SpinDownCall();
		}
		else if (pev->sequence != TURRET_ANIM_RETIRE)
		{
			SetTurretAnim(TURRET_ANIM_RETIRE);
			EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "turret/tu_retract.wav", TURRET_MACHINE_VOLUME, ATTN_NORM, 0, 120);
			SUB_UseTargets( this, USE_OFF, 0 );
		}
		else if (m_fSequenceFinished)
		{
			if (m_pEyeGlow)
				EyeOff();// XDM: moved here

			SetEyeBrightness(0);// XDM
			m_iOn = 0;
			m_flLastSight = 0;
			SetTurretAnim(TURRET_ANIM_NONE);
			pev->maxs.z = m_iRetractHeight;
			pev->mins.z = -m_iRetractHeight;
			UTIL_SetSize(pev, pev->mins, pev->maxs);
			if (m_iAutoStart)
			{
				SetThink(&CBaseTurret::AutoSearchThink);
				pev->nextthink = gpGlobals->time + 0.1f;
			}
			else
				SetThink(&CBaseEntity::SUB_DoNothing);
		}
	}
	else
	{
		SetTurretAnim(TURRET_ANIM_SPIN);
	}
}

void CBaseTurret::SetTurretAnim(TURRET_ANIM anim)
{
	if (pev->sequence != anim)
	{
		switch(anim)
		{
		case TURRET_ANIM_FIRE:
		case TURRET_ANIM_SPIN:
			if (pev->sequence != TURRET_ANIM_FIRE && pev->sequence != TURRET_ANIM_SPIN)
			{
				pev->frame = 0;
			}
			break;
		default:
			pev->frame = 0;
			break;
		}

		pev->sequence = anim;
		ResetSequenceInfo();

		switch(anim)
		{
		case TURRET_ANIM_RETIRE:
			pev->frame			= 255;
			pev->framerate		= -1.0;
			break;
		case TURRET_ANIM_DIE:
			pev->framerate		= 1.0;
			break;
		}
		//ALERT(at_console, "Turret anim #%d\n", anim);
	}
}

//
// This search function will sit with the turret deployed and look for a new target.
// After a set amount of time, the barrel will spin down. After m_flMaxWait, the turret will
// retact.
//
void CBaseTurret::SearchThink(void)
{
	// ensure rethink
	SetTurretAnim(TURRET_ANIM_SPIN);
	StudioFrameAdvance();
	pev->nextthink = gpGlobals->time + 0.1;

	if (m_flSpinUpTime == 0 && m_flMaxSpin)
		m_flSpinUpTime = gpGlobals->time + m_flMaxSpin;

	Ping();

	// If we have a target and we're still healthy
	if (m_hEnemy != NULL)
	{
		if (!m_hEnemy->IsAlive())
			m_hEnemy = NULL;// Dead enemy forces a search for new one
	}

	// Acquire Target
	if (m_hEnemy == NULL)
	{
		Look(TURRET_RANGE);
		m_hEnemy = BestVisibleEnemy();
	}

	// If we've found a target, spin up the barrel and start to attack
	if (m_hEnemy != NULL)
	{
		m_flLastSight = 0;
		m_flSpinUpTime = 0;
		SetThink(&CBaseTurret::ActiveThink);
	}
	else
	{
		// Are we out of time, do we need to retract?
 		if (gpGlobals->time > m_flLastSight)
		{
			//Before we retrace, make sure that we are spun down.
			m_flLastSight = 0;
			m_flSpinUpTime = 0;
			SetThink(&CBaseTurret::Retire);
		}
		// should we stop the spin?
		else if ((m_flSpinUpTime) && (gpGlobals->time > m_flSpinUpTime))
		{
			SpinDownCall();
		}

		// generic hunt for new victims
		m_vecGoalAngles.y = (m_vecGoalAngles.y + 0.1 * m_fTurnRate);
		if (m_vecGoalAngles.y >= 360)
			m_vecGoalAngles.y -= 360;
		MoveTurret();
	}
}

//
// This think function will deploy the turret when something comes into range. This is for
// automatically activated turrets.
//
void CBaseTurret::AutoSearchThink(void)
{
	// ensure rethink
	StudioFrameAdvance();
	pev->nextthink = gpGlobals->time + 0.3;

	// If we have a target and we're still healthy
	if (m_hEnemy != NULL)
	{
		if (!m_hEnemy->IsAlive() || m_hEnemy->edict() == pev->owner)// XDM: don't attack player!!
			m_hEnemy = NULL;// Dead enemy forces a search for new one
	}

	// Acquire Target
	if (m_hEnemy == NULL)
	{
		Look( TURRET_RANGE );
		m_hEnemy = BestVisibleEnemy();
	}

	if (m_hEnemy != NULL)
	{
		SetThink(&CBaseTurret::Deploy);
		EMIT_SOUND(ENT(pev), CHAN_BODY, "turret/tu_alert.wav", TURRET_MACHINE_VOLUME, ATTN_NORM);
	}
}

void CBaseTurret::TurretDeath(void)
{
	if (m_fSequenceFinished && !MoveTurret())
	{
		pev->framerate = 0;
		SetThinkNull();
		pev->nextthink = 0;
	}
	else
	{
		StudioFrameAdvance();
		pev->nextthink = gpGlobals->time + 0.1;

		if (RANDOM_LONG(0, 8) == 0)
		{
			Vector VecSrc;
			VecSrc.x = RANDOM_FLOAT( pev->absmin.x, pev->absmax.x );
			VecSrc.y = RANDOM_FLOAT( pev->absmin.y, pev->absmax.y );
			VecSrc.z = RANDOM_FLOAT( pev->absmin.z, pev->absmax.z );

			FX_Trail(VecSrc, RANDOM_LONG(20,30), FX_BLACKSMOKE);
		}

		if (RANDOM_LONG(0, 5) == 0)
		{
			Vector vecSrc = Vector(RANDOM_FLOAT(pev->absmin.x, pev->absmax.x), RANDOM_FLOAT(pev->absmin.y, pev->absmax.y), 0.0f);
			if (m_iOrientation == 0)
				vecSrc += Vector(0.0f, 0.0f, RANDOM_FLOAT(pev->origin.z, pev->absmax.z));
			else
				vecSrc += Vector(0.0f, 0.0f, RANDOM_FLOAT(pev->absmin.z, pev->origin.z));

			UTIL_Sparks(vecSrc);
		}
	}

/*	StudioFrameAdvance();
	pev->nextthink = gpGlobals->time + 0.1;

	if (pev->deadflag != DEAD_DEAD)
	{
		STOP_SOUND(ENT(pev), CHAN_STATIC, "turret/tu_active2.wav");
		DeathSound();
		pev->deadflag = DEAD_DEAD;

		if (m_iOrientation == 0)
			m_vecGoalAngles.x = -15;
		else
			m_vecGoalAngles.x = -90;

		SetTurretAnim(TURRET_ANIM_DIE);
		EyeOn();
	}

	EyeOff();

	if (pev->dmgtime + RANDOM_FLOAT(0, 2) > gpGlobals->time)
	{
		// lots of smoke
		MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
			WRITE_BYTE( TE_SMOKE );
			WRITE_COORD( RANDOM_FLOAT( pev->absmin.x, pev->absmax.x ) );
			WRITE_COORD( RANDOM_FLOAT( pev->absmin.y, pev->absmax.y ) );
			WRITE_COORD( pev->origin.z - m_iOrientation * 64 );
			WRITE_SHORT( g_iModelIndexSmoke );
			WRITE_BYTE( 25 ); // scale * 10
			WRITE_BYTE( 10 - m_iOrientation * 5); // framerate
		MESSAGE_END();
	}

	if (pev->dmgtime + RANDOM_FLOAT(0, 5) > gpGlobals->time)
	{
		Vector vecSrc = Vector( RANDOM_FLOAT( pev->absmin.x, pev->absmax.x ), RANDOM_FLOAT( pev->absmin.y, pev->absmax.y ), 0.0f );
		if (m_iOrientation == 0)
			vecSrc = vecSrc + Vector( 0.0f, 0.0f, RANDOM_FLOAT( pev->origin.z, pev->absmax.z ) );
		else
			vecSrc = vecSrc + Vector( 0.0f, 0.0f, RANDOM_FLOAT( pev->absmin.z, pev->origin.z ) );

		UTIL_Sparks( vecSrc );
	}

	if (m_fSequenceFinished && !MoveTurret( ) && pev->dmgtime + 5 < gpGlobals->time)
	{
		pev->framerate = 0;
		SetThinkNull();
		pev->nextthink = 0;
	}*/
}

void CBaseTurret::TraceAttack(CBaseEntity *pAttacker, float flDamage, const Vector &vecDir, TraceResult *ptr, int bitsDamageType)
{
	if (ptr->iHitgroup == HITGROUP_ARMOR)
	{
		// hit armor
		if ( pev->dmgtime != gpGlobals->time || (RANDOM_LONG(0,10) < 1) )
		{
			UTIL_Ricochet( ptr->vecEndPos, RANDOM_FLOAT( 1, 2) );
			pev->dmgtime = gpGlobals->time;
		}
		flDamage = 0.1;// don't hurt the monster much, but allow bits_COND_LIGHT_DAMAGE to be generated
	}

	if ( !pev->takedamage )
		return;

	AddMultiDamage( pAttacker, this, flDamage, bitsDamageType );
}

// take damage. bitsDamageType indicates type of damage sustained, ie: DMG_BULLET
int CBaseTurret::TakeDamage(CBaseEntity *pInflictor, CBaseEntity *pAttacker, float flDamage, int bitsDamageType)
{
	if (!m_iOn)
		flDamage *= 0.1f;

	// XDM3035a: we MUST call this in order to use Killed() and MonsterKilled()
	int iRet = CBaseMonster::TakeDamage(pInflictor, pAttacker, flDamage, bitsDamageType);

	if (iRet && !HasMemory(bits_MEMORY_KILLED))
	{
		if (pev->health > 0)
		{
			if (m_iOn)
			{
				m_fBeserk = 1;
				SetThink(&CBaseTurret::SearchThink);
			}
		}
	}
	return iRet;
}

void CBaseTurret::Killed(CBaseEntity *pInflictor, CBaseEntity *pAttacker, int iGib)
{
	SetThinkNull();
	SetTouchNull();
	EyeOff();
	STOP_SOUND(ENT(pev), CHAN_STATIC, "turret/tu_active2.wav");
	if (m_iOrientation == 0)
		m_vecGoalAngles.x = -15;
	else
		m_vecGoalAngles.x = -90;

	CBaseMonster::Killed(pInflictor, pAttacker, iGib);
	pev->takedamage = DAMAGE_NO;
//	pev->deadflag = DEAD_DYING;
	pev->deadflag = DEAD_DEAD;
	DeathSound();
//	SetTurretAnim(TURRET_ANIM_DIE);
//	pev->solid = SOLID_BBOX;
	pev->movetype = MOVETYPE_NONE;
	pev->angles.y = UTIL_AngleMod(pev->angles.y + RANDOM_LONG(0, 2) * 120);
	m_IdealActivity = ACT_DIESIMPLE;
	ChangeSchedule(GetScheduleOfType(SCHED_DIE));
	MaintainSchedule();//RunTask();
	ClearBits(pev->flags, FL_MONSTER); // why are they set in the first place???
	SetUseNull();
	SUB_UseTargets(this, USE_ON, 0); // wake up others
	SetThink(&CBaseTurret::TurretDeath);
	pev->nextthink = gpGlobals->time + 0.1;
}

int CBaseTurret::MoveTurret(void)
{
	int state = 0;
	// any x movement?
	if (m_vecCurAngles.x != m_vecGoalAngles.x)
	{
		float flDir = m_vecGoalAngles.x > m_vecCurAngles.x ? 1 : -1;

		m_vecCurAngles.x += 0.1 * m_fTurnRate * flDir;

		// if we started below the goal, and now we're past, peg to goal
		if (flDir == 1)
		{
			if (m_vecCurAngles.x > m_vecGoalAngles.x)
				m_vecCurAngles.x = m_vecGoalAngles.x;
		}
		else
		{
			if (m_vecCurAngles.x < m_vecGoalAngles.x)
				m_vecCurAngles.x = m_vecGoalAngles.x;
		}

		if (m_iOrientation == 0)
			SetBoneController(1, -m_vecCurAngles.x);
		else
			SetBoneController(1, m_vecCurAngles.x);
		state = 1;
	}

	if (m_vecCurAngles.y != m_vecGoalAngles.y)
	{
		float flDir = m_vecGoalAngles.y > m_vecCurAngles.y ? 1 : -1 ;
		float flDist = fabs(m_vecGoalAngles.y - m_vecCurAngles.y);

		if (flDist > 180)
		{
			flDist = 360 - flDist;
			flDir = -flDir;
		}
		if (flDist > 30)
		{
			if (m_fTurnRate < m_iBaseTurnRate * 10)
				m_fTurnRate += m_iBaseTurnRate;
		}
		else if (m_fTurnRate > 45)
		{
			m_fTurnRate -= m_iBaseTurnRate;
		}
		else
		{
			m_fTurnRate += m_iBaseTurnRate;
		}

		m_vecCurAngles.y += 0.1 * m_fTurnRate * flDir;

		if (m_vecCurAngles.y < 0)
			m_vecCurAngles.y += 360;
		else if (m_vecCurAngles.y >= 360)
			m_vecCurAngles.y -= 360;

		if (flDist < (0.05 * m_iBaseTurnRate))
			m_vecCurAngles.y = m_vecGoalAngles.y;

		//ALERT(at_console, "%.2f -> %.2f\n", m_vecCurAngles.y, y);
		if (m_iOrientation == 0)
			SetBoneController(0, m_vecCurAngles.y - pev->angles.y );
		else
			SetBoneController(0, pev->angles.y - 180 - m_vecCurAngles.y );
		state = 1;
	}

	if (!state)
		m_fTurnRate = m_iBaseTurnRate;

	//ALERT(at_console, "(%.2f, %.2f)->(%.2f, %.2f)\n", m_vecCurAngles.x,
	//	m_vecCurAngles.y, m_vecGoalAngles.x, m_vecGoalAngles.y);
	return state;
}

//
// ID as a machine
//
int	CBaseTurret::Classify (void)
{
	if (m_iOn || m_iAutoStart)
	{
		if(pev->impulse)// XDM: used by player
			return CLASS_PLAYER_ALLY;
		else
			return m_iClass?m_iClass:CLASS_MACHINE;
	}

	return CLASS_NONE;
}

void CBaseTurret::AlertSound(void)
{
	EMIT_SOUND(ENT(pev), CHAN_BODY, "turret/tu_deploy.wav", TURRET_MACHINE_VOLUME, ATTN_NORM);
}

void CBaseTurret::PainSound(void)
{
	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, gSoundsMetal[RANDOM_LONG(0,NUM_SHARD_SOUNDS-1)], VOL_NORM, ATTN_IDLE, 0, RANDOM_LONG(95,105));
}

void CBaseTurret::DeathSound(void)
{
	if (m_iOn)// XDM: play different sounds according to sate
	{
		if (RANDOM_LONG(0,1) == 0)
			EMIT_SOUND(ENT(pev), CHAN_BODY, "turret/tu_die2.wav", TURRET_MACHINE_VOLUME, ATTN_NORM);
		else
			EMIT_SOUND(ENT(pev), CHAN_BODY, "turret/tu_die3.wav", TURRET_MACHINE_VOLUME, ATTN_NORM);
	}
	else
		EMIT_SOUND(ENT(pev), CHAN_BODY, "turret/tu_die.wav", TURRET_MACHINE_VOLUME, ATTN_NORM);
}




class CTurret : public CBaseTurret
{
public:
	virtual void Spawn(void);
	virtual void Precache(void);
	// Think functions
	virtual void SpinUpCall(void);
	virtual void SpinDownCall(void);
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
	// other functions
	void Shoot(Vector &vecSrc, Vector &vecDirToEnemy);

private:
	int m_iStartSpin;
};

TYPEDESCRIPTION	CTurret::m_SaveData[] =
{
	DEFINE_FIELD( CTurret, m_iStartSpin, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CTurret, CBaseTurret );

LINK_ENTITY_TO_CLASS( monster_turret, CTurret );

void CTurret::Spawn(void)
{
	Precache();
	SET_MODEL(ENT(pev), "models/turret.mdl");
	pev->health			= gSkillData.turretHealth;
	m_HackedGunPos		= Vector(0.0f, 0.0f, 12.75f);
	m_flMaxSpin =		TURRET_MAXSPIN;
	CBaseTurret::Spawn();
	m_iRetractHeight = 16;
	m_iDeployHeight = 32;
	m_iMinPitch	= -15;
	if (pev->view_ofs == g_vecZero)
		pev->view_ofs.z = 12.75;// default
	UTIL_SetSize(pev, Vector(-32, -32, -m_iRetractHeight), Vector(32, 32, m_iRetractHeight));

	m_pEyeGlow = CSprite::SpriteCreate(TURRET_GLOW_SPRITE, pev->origin, FALSE);
	if (m_pEyeGlow)
	{
		m_pEyeGlow->SetTransparency(kRenderGlow, 255, 255, 255, 0, kRenderFxNoDissipation);
		m_pEyeGlow->SetAttachment(edict(), 2);
	}
	m_eyeBrightness = 0;
	SetThink(&CBaseTurret::Initialize);
	pev->nextthink = gpGlobals->time + 0.3;
}

void CTurret::Precache(void)
{
	CBaseTurret::Precache();
	PRECACHE_MODEL("models/turret.mdl");
	PRECACHE_MODEL(TURRET_GLOW_SPRITE);
}

void CTurret::Shoot(Vector &vecSrc, Vector &vecDirToEnemy)
{
	if (!m_iSpin)// XDM: don't fire when the barrel is not spinning
	{
		SetThink(&CTurret::SpinUpCall);
		return;// XDM3035b
	}

	FireBullets(1, vecSrc, vecDirToEnemy, TURRET_SPREAD, NULL, TURRET_RANGE, BULLET_HEAVY_TURRET, gSkillData.DmgHeavyTurret, DMG_BULLET, this, m_hActivator, 0);
	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "weapons/fire_heavy_turret.wav", VOL_NORM, ATTN_LOW_HIGH, 0, RANDOM_LONG(95,105));
	pev->effects = pev->effects | EF_MUZZLEFLASH;
}

void CTurret::SpinUpCall(void)
{
	StudioFrameAdvance();
	pev->nextthink = gpGlobals->time + 0.1;

	// Are we already spun up? If not start the two stage process.
	if (!m_iSpin)
	{
//		m_flInSpinProc = TRUE;// XDM
		SetTurretAnim(TURRET_ANIM_SPIN);
		// for the first pass, spin up the the barrel
		if (!m_iStartSpin)
		{
			pev->nextthink = gpGlobals->time + 1.0; // spinup delay
			EMIT_SOUND(ENT(pev), CHAN_BODY, "turret/tu_spinup.wav", TURRET_MACHINE_VOLUME, ATTN_STATIC);
			m_iStartSpin = 1;
			if (pev->framerate >= 1.0f)// only restart if was not half-way here
				pev->framerate = 0.1;
		}
		// after the barrel is spun up, turn on the hum
		else if (pev->framerate >= 1.0)
		{
			pev->nextthink = gpGlobals->time + 0.1; // retarget delay
			EMIT_SOUND(ENT(pev), CHAN_STATIC, "turret/tu_active2.wav", TURRET_MACHINE_VOLUME, ATTN_STATIC);
			SetThink(&CBaseTurret::ActiveThink);
			m_iStartSpin = 0;
			m_iSpin = 1;
		}
		else
			pev->framerate += 0.075;
	}

	if (m_iSpin)
	{
//		m_flInSpinProc = FALSE;// XDM
		SetThink(&CBaseTurret::ActiveThink);
	}
}

void CTurret::SpinDownCall(void)
{
	if (m_iSpin)
	{
//		m_flInSpinProc = TRUE;// XDM
		SetTurretAnim(TURRET_ANIM_SPIN);
		if (pev->framerate >= 1.0f)
		{
			STOP_SOUND(ENT(pev), CHAN_STATIC, "turret/tu_active2.wav");
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "turret/tu_spindown.wav", TURRET_MACHINE_VOLUME, ATTN_STATIC);
			m_iSpin = 0;// XDM3035b let's think we'll have to spinup again
		}
	}
	if (pev->framerate > 0.0f)// XDM3035b: manage framerate separately
	{
		pev->framerate -= 0.04;
//		if (pev->framerate <= 0.4f)// XDM3035b let's think we'll have to spinup again
//			m_iSpin = 0;

		if (pev->framerate <= 0)
		{
			pev->framerate = 0;
			m_iSpin = 0;
//			m_flInSpinProc = FALSE;// XDM
		}
	}
}




class CMiniTurret : public CBaseTurret
{
public:
	virtual void Spawn(void);
	virtual void Precache(void);
	virtual void Shoot(Vector &vecSrc, Vector &vecDirToEnemy);
};

LINK_ENTITY_TO_CLASS( monster_miniturret, CMiniTurret );

void CMiniTurret::Spawn(void)
{
	Precache();
	SET_MODEL(ENT(pev), "models/miniturret.mdl");
	pev->health = gSkillData.miniturretHealth;
	m_HackedGunPos = Vector(0.0f, 0.0f, 12.75f);
	m_flMaxSpin = 0;
	CBaseTurret::Spawn();
	m_iRetractHeight = 16;
	m_iDeployHeight = 32;
	m_iMinPitch	= -15;
	if (pev->view_ofs == g_vecZero)
		pev->view_ofs.z = 12.75;// default
	UTIL_SetSize(pev, Vector(-16, -16, -m_iRetractHeight), Vector(16, 16, m_iRetractHeight));
	SetThink(&CBaseTurret::Initialize);
	pev->nextthink = gpGlobals->time + 0.3;
}

void CMiniTurret::Precache(void)
{
	CBaseTurret::Precache();
	PRECACHE_MODEL("models/miniturret.mdl");
}

void CMiniTurret::Shoot(Vector &vecSrc, Vector &vecDirToEnemy)
{
	FireBullets(1, vecSrc, vecDirToEnemy, TURRET_SPREAD, NULL, TURRET_RANGE, BULLET_MACHINEGUN, gSkillData.DmgMachinegun, DMG_BULLET, this, m_hActivator, 0);
	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "weapons/fire_machinegun.wav", VOL_NORM, ATTN_LOW_HIGH, 0, RANDOM_LONG(95,105));
	pev->effects = pev->effects | EF_MUZZLEFLASH;
}

// BUGBUG: triggered monster_sentry is non-solid!!!

//=========================================================
// Sentry gun - smallest turret, placed near grunt entrenchments
//=========================================================
class CSentry : public CBaseTurret
{
public:
	virtual void Spawn(void);
	virtual void Precache(void);
	virtual int TakeDamage(CBaseEntity *pInflictor, CBaseEntity *pAttacker, float flDamage, int bitsDamageType);
	virtual void Killed(CBaseEntity *pInflictor, CBaseEntity *pAttacker, int iGib);// XDM3035a
	virtual void AlignToFloor(void) {};// XDM3035b: do nothing
	virtual void Shoot(Vector &vecSrc, Vector &vecDirToEnemy);
	virtual void AlertSound(void);// XDM
	virtual BOOL IsPushable(void) { return TRUE; }

	void EXPORT SentryTouch(CBaseEntity *pOther);
	void EXPORT SentryDeath(void);
};

LINK_ENTITY_TO_CLASS( monster_sentry, CSentry );

void CSentry::Precache(void)
{
//	CBaseTurret::Precache();
	PRECACHE_MODEL("models/sentry.mdl");
	PRECACHE_SOUND("turret/tu_alert.wav");
	PRECACHE_SOUND("turret/tu_deploy_short.wav");
	PRECACHE_SOUND("turret/tu_die.wav");
	PRECACHE_SOUND("turret/tu_die2.wav");
	PRECACHE_SOUND("turret/tu_die3.wav");
	PRECACHE_SOUND("turret/tu_ping.wav");
}

void CSentry::Spawn(void)
{
	Precache();
	SET_MODEL(ENT(pev), "models/sentry.mdl");
	pev->health			= gSkillData.sentryHealth;
	m_HackedGunPos		= Vector(0, 0, 48);

	m_flMaxWait = 1E6;
	m_flMaxSpin	= 1E6;
	CBaseTurret::Spawn();
	m_iRetractHeight = 64;
	m_iDeployHeight = 64;
	m_iMinPitch	= -60;
	if (pev->view_ofs == g_vecZero)
		pev->view_ofs.z = 48;// default
	UTIL_SetSize(pev, Vector(-16, -16, 0), Vector(16, 16, 32));// XDM
	SetTouch(&CSentry::SentryTouch);
	SetThink(&CBaseTurret::Initialize);
	pev->nextthink = gpGlobals->time + 0.3;
}

void CSentry::Shoot(Vector &vecSrc, Vector &vecDirToEnemy)
{
	FireBullets(1, vecSrc, vecDirToEnemy, TURRET_SPREAD, NULL, TURRET_RANGE, BULLET_MINIGUN, gSkillData.DmgMachinegun, DMG_BULLET, this, m_hActivator, 0);
	EMIT_SOUND(ENT(pev), CHAN_WEAPON, "weapons/fire_minigun.wav", VOL_NORM, ATTN_NORM);
	pev->effects = pev->effects | EF_MUZZLEFLASH;
}

int CSentry::TakeDamage(CBaseEntity *pInflictor, CBaseEntity *pAttacker, float flDamage, int bitsDamageType)
{
	// XDM3035c: don't call CBaseTurret::TakeDamage() because it nullifies damage in disabled state
	int iRet = CBaseMonster::TakeDamage(pInflictor, pAttacker, flDamage, bitsDamageType);

	if (iRet && !HasMemory(bits_MEMORY_KILLED) && pev->health > 0)
	{
		if (m_iOn)
		{
			m_fBeserk = 1;
			SetThink(&CBaseTurret::SearchThink);
		}
		else
		{
			SetThink(&CBaseTurret::Deploy);
			SetUseNull();
			pev->nextthink = gpGlobals->time + 0.1;
		}
	}
	return 1;
}

void CSentry::Killed(CBaseEntity *pInflictor, CBaseEntity *pAttacker, int iGib)// XDM3035a
{
	SetThinkNull();
	SetTouchNull();
	SetBoneController(0, 0);
	SetBoneController(1, 0);
	CBaseMonster::Killed(pInflictor, pAttacker, iGib);
	pev->takedamage = DAMAGE_NO;
//	pev->deadflag = DEAD_DYING;
	pev->deadflag = DEAD_DEAD;
	DeathSound();
//	SetTurretAnim(TURRET_ANIM_DIE);
	UTIL_SetSize(pev, Vector(-2, -2, 0), Vector(2, 2, 8));// XDM
	pev->solid = SOLID_BBOX;
	pev->movetype = MOVETYPE_TOSS;
	pev->angles.y = UTIL_AngleMod(pev->angles.y + RANDOM_LONG(0, 2) * 120);
	m_IdealActivity = ACT_DIESIMPLE;
	ChangeSchedule(GetScheduleOfType(SCHED_DIE));
	MaintainSchedule();//RunTask();

//	CBaseTurret::Killed(pInflictor, pAttacker, iGib);
	SetThink(&CSentry::SentryDeath);
	pev->nextthink = gpGlobals->time + 0.1;
}

void CSentry::SentryTouch(CBaseEntity *pOther)
{
	if (pOther && (pOther->IsPlayer() || pOther->IsMonster()))// pev->flags & FL_MONSTER)))
		TakeDamage(pOther, pOther, 0, 0);
}

void CSentry::SentryDeath(void)
{
	if (m_fSequenceFinished)
	{
		pev->framerate = 0;
		SetThinkNull();
		pev->nextthink = 0;
	}
	else
	{
		StudioFrameAdvance();// for m_fSequenceFinished
		pev->nextthink = gpGlobals->time + 0.1;
		Vector vecSrc, vecAng;
		GetAttachment(1, vecSrc, vecAng);

		if (RANDOM_LONG(0,8) == 0)
		{
			Vector VecSrc;
			VecSrc.x = RANDOM_FLOAT( pev->absmin.x, pev->absmax.x );
			VecSrc.y = RANDOM_FLOAT( pev->absmin.y, pev->absmax.y );
			VecSrc.z = RANDOM_FLOAT( pev->absmin.z, pev->absmax.z );

			FX_Trail(VecSrc, RANDOM_LONG(20,30), FX_BLACKSMOKE);
		}
		if (RANDOM_LONG(0,6) == 0)
			UTIL_Sparks(vecSrc);
	}
}

void CSentry::AlertSound(void)
{
	EMIT_SOUND(ENT(pev), CHAN_BODY, "turret/tu_deploy_short.wav", TURRET_MACHINE_VOLUME, ATTN_NORM);
}
