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
#ifndef SCRIPTED_H
#define SCRIPTED_H

#ifndef SCRIPTEVENT_H
#include "scriptevent.h"
#endif

#define SF_SCRIPT_WAITTILLSEEN		1
#define SF_SCRIPT_EXITAGITATED		2
#define SF_SCRIPT_REPEATABLE		4
#define SF_SCRIPT_LEAVECORPSE		8
//#define SF_SCRIPT_INTERPOLATE		16 // don't use, old bug
#define SF_SCRIPT_NOINTERRUPT		32
#define SF_SCRIPT_OVERRIDESTATE		64
#define SF_SCRIPT_NOSCRIPTMOVEMENT	128

#define SCRIPT_BREAK_CONDITIONS		(bits_COND_LIGHT_DAMAGE|bits_COND_HEAVY_DAMAGE)

enum SS_INTERRUPT
{
	SS_INTERRUPT_IDLE = 0,
	SS_INTERRUPT_BY_NAME,
	SS_INTERRUPT_AI,
};

#define SSCLASSNAME "scripted_sequence"

// when a monster finishes an AI scripted sequence, we can choose
// a schedule to place them in. These defines are the aliases to
// resolve worldcraft input to real schedules (sjb)
#define SCRIPT_FINISHSCHED_DEFAULT	0
#define SCRIPT_FINISHSCHED_AMBUSH	1

class CCineMonster : public CBaseMonster
{
public:
	virtual void Spawn( void );
	virtual void KeyValue( KeyValueData *pkvd );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual void Blocked( CBaseEntity *pOther );
	virtual void Touch( CBaseEntity *pOther );
	virtual int	 ObjectCaps( void ) { return (CBaseMonster :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION); }
	virtual void Activate( void );
	virtual void Killed(CBaseEntity *pInflictor, CBaseEntity *pAttacker, int iGib);

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );

	static	TYPEDESCRIPTION m_SaveData[];

	// void EXPORT CineSpawnThink( void );
	void EXPORT CineThink( void );
//	void Pain( void );
//	void Die( void );
	void DelayStart( int state );
	BOOL FindEntity( void );
	virtual void PossessEntity( void );

	void ReleaseEntity( CBaseMonster *pEntity );
	void CancelScript( void );
	virtual BOOL StartSequence( CBaseMonster *pTarget, int iszSeq, BOOL completeOnEmpty );
	virtual BOOL FCanOverrideState ( void );
	void SequenceDone ( CBaseMonster *pMonster );
	virtual void FixScriptMonsterSchedule( CBaseMonster *pMonster );
	BOOL	CanInterrupt( void );
	void	AllowInterrupt( BOOL fAllow );
	int		IgnoreConditions( void );

	int	m_iszIdle;		// string index for idle animation
	int	m_iszPlay;		// string index for scripted animation
	int m_iszEntity;	// entity that is wanted for this script
	int m_fMoveTo;
	int m_iFinishSchedule;
	float m_flRadius;		// range to search
	float m_flRepeat;	// repeat rate

	int m_iDelay;
	float m_startTime;

	int	m_saved_movetype;
	int	m_saved_solid;
	int m_saved_effects;
//	Vector m_vecOrigOrigin;
	BOOL m_interruptable;
};


class CCineAI : public CCineMonster
{
	virtual BOOL StartSequence(CBaseMonster *pTarget, int iszSeq, BOOL completeOnEmpty);
	virtual void PossessEntity(void);
	virtual BOOL FCanOverrideState(void);
	virtual void FixScriptMonsterSchedule(CBaseMonster *pMonster);
};


#define SF_SENTENCE_ONCE		0x0001
#define SF_SENTENCE_FOLLOWERS	0x0002	// only say if following player
#define SF_SENTENCE_INTERRUPT	0x0004	// force talking except when dead
#define SF_SENTENCE_CONCURRENT	0x0008	// allow other people to keep talking

class CScriptedSentence : public CBaseToggle
{
public:
	virtual void Spawn( void );
	virtual void KeyValue( KeyValueData *pkvd );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void EXPORT FindThink( void );
	void EXPORT DelayThink( void );
	virtual int ObjectCaps( void ) { return (CBaseToggle :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION); }

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );

	static	TYPEDESCRIPTION m_SaveData[];

	CBaseMonster *FindEntity( void );
	BOOL AcceptableSpeaker( CBaseMonster *pMonster );
	BOOL StartSentence( CBaseMonster *pTarget );


private:
	int		m_iszSentence;		// string index for idle animation
	int		m_iszEntity;	// entity that is wanted for this sentence
	float	m_flRadius;		// range to search
	float	m_flDuration;	// How long the sentence lasts
	float	m_flRepeat;	// repeat rate
	float	m_flAttenuation;
	float	m_flVolume;
	BOOL	m_active;
	int		m_iszListener;	// name of entity to look at while talking
};


class CFurniture : public CBaseMonster
{
public:
	virtual void Spawn (void);
	virtual void Killed(CBaseEntity *pInflictor, CBaseEntity *pAttacker, int iGib);
	virtual int Classify(void);
	virtual int	ObjectCaps(void) { return (CBaseMonster :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION); }
};

#endif		//SCRIPTED_H
