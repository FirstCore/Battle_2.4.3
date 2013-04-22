/*
 * Copyright (C) 2013  BlizzLikeGroup
 * BlizzLikeCore integrates as part of this file: CREDITS.md and LICENSE.md
 */

/* ScriptData
Name: Boss_Darkmaster_Gandling
Complete(%): 99
Comment: Doors missing in instance script.
Category: Scholomance
EndScriptData */

#include "ScriptPCH.h"
#include "scholomance.h"

#define SPELL_ARCANEMISSILES           22272
#define SPELL_SHADOWSHIELD             22417                //Not right ID. But 12040 is wrong either.
#define SPELL_CURSE                    18702

#define ADD_1X 170.205f
#define ADD_1Y 99.413f
#define ADD_1Z 104.733f
#define ADD_1O 3.16f

#define ADD_2X 170.813f
#define ADD_2Y 97.857f
#define ADD_2Z 104.713f
#define ADD_2O 3.16f

#define ADD_3X 170.720f
#define ADD_3Y 100.900f
#define ADD_3Z 104.739f
#define ADD_3O 3.16f

#define ADD_4X 171.866f
#define ADD_4Y 99.373f
#define ADD_4Z 104.732f
#define ADD_4O 3.16f

struct boss_darkmaster_gandlingAI : public ScriptedAI
{
    boss_darkmaster_gandlingAI(Creature* c) : ScriptedAI(c)
    {
        pInstance = me->GetInstanceData();
    }

    ScriptedInstance* pInstance;

    uint32 ArcaneMissiles_Timer;
    uint32 ShadowShield_Timer;
    uint32 Curse_Timer;
    uint32 Teleport_Timer;

    void Reset()
    {
        ArcaneMissiles_Timer = 4500;
        ShadowShield_Timer = 12000;
        Curse_Timer = 2000;
        Teleport_Timer = 16000;
    }

    void EnterCombat(Unit* /*who*/)
    {
    }

    void JustDied(Unit* /*killer*/)
    {
        if (pInstance)
            pInstance->SetData(TYPE_GANDLING, DONE);
    }

    void UpdateAI(const uint32 diff)
    {
        if (!UpdateVictim())
            return;

        //ArcaneMissiles_Timer
        if (ArcaneMissiles_Timer <= diff)
        {
            DoCast(me->getVictim(), SPELL_ARCANEMISSILES);
            ArcaneMissiles_Timer = 8000;
        } else ArcaneMissiles_Timer -= diff;

        //ShadowShield_Timer
        if (ShadowShield_Timer <= diff)
        {
            DoCast(me, SPELL_SHADOWSHIELD);
            ShadowShield_Timer = 14000 + rand()%14000;
        } else ShadowShield_Timer -= diff;

        //Curse_Timer
        if (Curse_Timer <= diff)
        {
            DoCast(me->getVictim(), SPELL_CURSE);
            Curse_Timer = 15000 + rand()%12000;
        } else Curse_Timer -= diff;

        //Teleporting Random Target to one of the six pre boss rooms and spawn 3-4 skeletons near the gamer.
        //We will only telport if gandling has more than 3% of hp so teleported gamers can always loot.
        if (me->GetHealth()*100 / me->GetMaxHealth() > 3)
        {
            if (Teleport_Timer <= diff)
            {
                Unit* pTarget = NULL;
                pTarget = SelectUnit(SELECT_TARGET_RANDOM,0);
                if (pTarget && pTarget->GetTypeId() == TYPEID_PLAYER)
                {
                    if (DoGetThreat(pTarget))
                        DoModifyThreatPercent(pTarget, -100);

                    Creature* Summoned = NULL;
                    switch(rand()%6)
                    {
                        case 0:
                            DoTeleportPlayer(pTarget, 250.0696f,0.3921f,84.8408f,3.149f);
                            Summoned = me->SummonCreature(16119,254.2325f,0.3417f,84.8407f,0,TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,10000);
                            if (Summoned)
                                Summoned->AI()->AttackStart(pTarget);
                            Summoned = me->SummonCreature(16119,257.7133f,4.0226f,84.8407f,0,TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,10000);
                            if (Summoned)
                                Summoned->AI()->AttackStart(pTarget);
                            Summoned = me->SummonCreature(16119,258.6702f,-2.60656f,84.8407f,0,TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,10000);
                            if (Summoned)
                                Summoned->AI()->AttackStart(pTarget);
                            break;
                        case 1:
                            DoTeleportPlayer(pTarget, 181.4220f,-91.9481f,84.8410f,1.608f);
                            Summoned = me->SummonCreature(16119,184.0519f,-73.5649f,84.8407f,0,TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,10000);
                            if (Summoned)
                                Summoned->AI()->AttackStart(pTarget);
                            Summoned = me->SummonCreature(16119,179.5951f,-73.7045f,84.8407f,0,TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,10000);
                            if (Summoned)
                                Summoned->AI()->AttackStart(pTarget);
                            Summoned = me->SummonCreature(16119,180.6452f,-78.2143f,84.8407f,0,TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,10000);
                            if (Summoned)
                                Summoned->AI()->AttackStart(pTarget);
                            Summoned = me->SummonCreature(16119,283.2274f,-78.1518f,84.8407f,0,TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,10000);
                            if (Summoned)
                                Summoned->AI()->AttackStart(pTarget);
                            break;
                        case 2:
                            DoTeleportPlayer(pTarget, 95.1547f,-1.8173f,85.2289f,0.043f);
                            Summoned = me->SummonCreature(16119,100.9404f,-1.8016f,85.2289f,0,TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,10000);
                            if (Summoned)
                                Summoned->AI()->AttackStart(pTarget);
                            Summoned = me->SummonCreature(16119,101.3729f,0.4882f,85.2289f,0,TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,10000);
                            if (Summoned)
                                Summoned->AI()->AttackStart(pTarget);
                            Summoned = me->SummonCreature(16119,101.4596f,-4.4740f,85.2289f,0,TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,10000);
                            if (Summoned)
                                Summoned->AI()->AttackStart(pTarget);
                            break;
                        case 3:
                            DoTeleportPlayer(pTarget, 250.0696f,0.3921f,72.6722f,3.149f);
                            Summoned = me->SummonCreature(16119,240.34481f,0.7368f,72.6722f,0,TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,10000);
                            if (Summoned)
                                Summoned->AI()->AttackStart(pTarget);
                            Summoned = me->SummonCreature(16119,240.3633f,-2.9520f,72.6722f,0,TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,10000);
                            if (Summoned)
                                Summoned->AI()->AttackStart(pTarget);
                            Summoned = me->SummonCreature(16119,240.6702f,3.34949f,72.6722f,0,TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,10000);
                            if (Summoned)
                                Summoned->AI()->AttackStart(pTarget);
                            break;
                        case 4:
                            DoTeleportPlayer(pTarget, 181.4220f,-91.9481f,70.7734f,1.608f);
                            Summoned = me->SummonCreature(16119,184.0519f,-73.5649f,70.7734f,0,TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,10000);
                            if (Summoned)
                                Summoned->AI()->AttackStart(pTarget);
                            Summoned = me->SummonCreature(16119,179.5951f,-73.7045f,70.7734f,0,TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,10000);
                            if (Summoned)
                                Summoned->AI()->AttackStart(pTarget);
                            Summoned = me->SummonCreature(16119,180.6452f,-78.2143f,70.7734f,0,TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,10000);
                            if (Summoned)
                                Summoned->AI()->AttackStart(pTarget);
                            Summoned = me->SummonCreature(16119,283.2274f,-78.1518f,70.7734f,0,TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,10000);
                            if (Summoned)
                                Summoned->AI()->AttackStart(pTarget);
                            break;
                        case 5:
                            DoTeleportPlayer(pTarget, 106.1541f,-1.8994f,75.3663f,0.043f);
                            Summoned = me->SummonCreature(16119,115.3945f,-1.5555f,75.3663f,0,TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,10000);
                            if (Summoned)
                                Summoned->AI()->AttackStart(pTarget);
                            Summoned = me->SummonCreature(16119,257.7133f,1.8066f,75.3663f,0,TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,10000);
                            if (Summoned)
                                Summoned->AI()->AttackStart(pTarget);
                            Summoned = me->SummonCreature(16119,258.6702f,-5.1001f,75.3663f,0,TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,10000);
                            if (Summoned)
                                Summoned->AI()->AttackStart(pTarget);
                            break;
                    }
                }
                Teleport_Timer = 20000 + rand()%15000;
            } else Teleport_Timer -= diff;
        }

        DoMeleeAttackIfReady();
    }
};
CreatureAI* GetAI_boss_darkmaster_gandling(Creature* pCreature)
{
    return new boss_darkmaster_gandlingAI (pCreature);
}

void AddSC_boss_darkmaster_gandling()
{
    Script *newscript;
    newscript = new Script;
    newscript->Name = "boss_darkmaster_gandling";
    newscript->GetAI = &GetAI_boss_darkmaster_gandling;
    newscript->RegisterSelf();
}

