//====================================================================
//
// Purpose: MoveWith-related stuff
//
//====================================================================
#ifndef MOVEWITH_H
#define MOVEWITH_H
#ifdef _WIN32
#pragma once
#endif

#define LF_NOTEASY				(1<<0)
#define LF_NOTMEDIUM			(1<<1)
#define LF_NOTHARD				(1<<2)
#define LF_POSTASSISTVEL		(1<<3)
#define LF_POSTASSISTAVEL		(1<<4)
#define LF_DOASSIST				(1<<5)
#define LF_CORRECTSPEED			(1<<6)
#define LF_DODESIRED			(1<<7)
#define LF_DESIRED_THINK		(1<<8)
#define LF_DESIRED_POSTASSIST	(1<<9)
#define LF_DESIRED_INFO			(1<<10)
#define LF_DESIRED_ACTION		(1<<11)
#define LF_MOVENONE				(1<<12)
#define LF_MERGEPOS				(1<<13)
#define LF_PARENTMOVE			(1<<14)
#define LF_ANGULAR				(1<<15)
#define LF_POSTORG				(1<<16)
#define LF_POINTENTITY			(1<<17)
#define LF_ALIASLIST			(1<<18)

// an entity must have one of these flags set in order to be in the AssistList
#define LF_ASSISTLIST  (LF_DOASSIST|LF_DODESIRED|LF_MERGEPOS|LF_POSTORG)

bool NeedUpdate(CBaseEntity *pEnt);

void CheckDesiredList(void);
void CheckAssistList(void);

extern void UTIL_DesiredAction(CBaseEntity *pEnt);
extern void UTIL_DesiredThink(CBaseEntity *pEnt);
extern void UTIL_DesiredInfo(CBaseEntity *pEnt);
extern void UTIL_DesiredPostAssist(CBaseEntity *pEnt);
extern void UTIL_AddToAssistList(CBaseEntity *pEnt);
extern void UTIL_MarkForAssist(CBaseEntity *pEnt, bool correctSpeed);
extern void UTIL_AssignOrigin(CBaseEntity *pEntity, const Vector &vecOrigin);
extern void UTIL_AssignOrigin(CBaseEntity *pEntity, const Vector &vecOrigin, bool bInitiator);
extern void UTIL_SetVelocity(CBaseEntity *pEnt,	const Vector &vecSet);
extern void UTIL_AssignAngles(CBaseEntity *pEntity, const Vector &vecAngles);
extern void UTIL_AssignAngles(CBaseEntity *pEntity, const Vector &vecAngles, bool bInitiator);
extern void UTIL_SetMoveWithVelocity(CBaseEntity *pEnt, const Vector &vecSet, int loopbreaker);
extern void UTIL_SetMoveWithAvelocity(CBaseEntity *pEnt, const Vector &vecSet, int loopbreaker);
extern void UTIL_SetAvelocity(CBaseEntity *pEnt, const Vector &vecSet);
extern void UTIL_MergePos(CBaseEntity *pEnt, int loopbreaker = 100);

#endif // MOVEWITH_H
