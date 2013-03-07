/*
 * Copyright (C) 2011-2013 BlizzLikeCore <http://blizzlike.servegame.com/>
 * Please, read the credits file.
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/* ScriptData
Name: Boss_Epoch_Hunter
Complete(%): 60
Comment: Missing spawns pre-event, missing speech to be coordinated with rest of escort event.
Category: Caverns of Time, Old Hillsbrad Foothills
EndScriptData */

#include "ScriptPCH.h"
#include "old_hillsbrad.h"

#define SAY_ENTER1                  -1560013
#define SAY_ENTER2                  -1560014
#define SAY_ENTER3                  -1560015
#define SAY_AGGRO1                  -1560016
#define SAY_AGGRO2                  -1560017
#define SAY_SLAY1                   -1560018
#define SAY_SLAY2                   -1560019
#define SAY_BREATH1                 -1560020
#define SAY_BREATH2                 -1560021
#define SAY_DEATH                   -1560022

#define SPELL_SAND_BREATH           31914
#define SPELL_IMPENDING_DEATH       31916
#define SPELL_MAGIC_DISRUPTION_AURA 33834
#define SPELL_WING_BUFFET           31475

struct boss_epoch_hunterAI : public ScriptedAI
{
    boss_epoch_hunterAI(Creature *c) : ScriptedAI(c)
    {
        pInstance = c->GetInstanceData();
    }

    ScriptedInstance *pInstance;

    uint32 SandBreath_Timer;
    uint32 ImpendingDeath_Timer;
    uint32 WingBuffet_Timer;
    uint32 Mda_Timer;

    void Reset()
    {
        SandBreath_Timer = 25000;
        ImpendingDeath_Timer = 30000;
        WingBuffet_Timer = 35000;
        Mda_Timer = 40000;
    }

    void EnterCombat(Unit*)
    {
        switch(rand()%2)
        {
            case 0: DoScriptText(SAY_AGGRO1, me); break;
            case 1: DoScriptText(SAY_AGGRO2, me); break;
        }
    }

    void KilledUnit(Unit*)
    {
        switch(rand()%2)
        {
            case 0: DoScriptText(SAY_SLAY1, me); break;
            case 1: DoScriptText(SAY_SLAY2, me); break;
        }
    }

    void JustDied(Unit*)
    {
        DoScriptText(SAY_DEATH, me);

        if (pInstance && pInstance->GetData(TYPE_THRALL_EVENT) == IN_PROGRESS)
            pInstance->SetData(TYPE_THRALL_PART4, DONE);
    }

    void UpdateAI(const uint32 diff)
    {
        //Return since we have no target
        if (!UpdateVictim())
            return;

        //Sand Breath
        if (SandBreath_Timer <= diff)
        {
            if (me->IsNonMeleeSpellCasted(false))
                me->InterruptNonMeleeSpells(false);

            DoCast(me->getVictim(),SPELL_SAND_BREATH);

            switch(rand()%2)
            {
                case 0: DoScriptText(SAY_BREATH1, me); break;
                case 1: DoScriptText(SAY_BREATH2, me); break;
            }

            SandBreath_Timer = 25000+rand()%5000;
        } else SandBreath_Timer -= diff;

        if (ImpendingDeath_Timer <= diff)
        {
            DoCast(me->getVictim(),SPELL_IMPENDING_DEATH);
            ImpendingDeath_Timer = 30000+rand()%5000;
        } else ImpendingDeath_Timer -= diff;

        if (WingBuffet_Timer <= diff)
        {
            if (Unit* pTarget = SelectUnit(SELECT_TARGET_RANDOM,0))
                DoCast(pTarget,SPELL_WING_BUFFET);
            WingBuffet_Timer = 25000+rand()%10000;
        } else WingBuffet_Timer -= diff;

        if (Mda_Timer <= diff)
        {
            DoCast(me,SPELL_MAGIC_DISRUPTION_AURA);
            Mda_Timer = 15000;
        } else Mda_Timer -= diff;

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_epoch_hunter(Creature* pCreature)
{
    return new boss_epoch_hunterAI (pCreature);
}

void AddSC_boss_epoch_hunter()
{
    Script *newscript;
    newscript = new Script;
    newscript->Name = "boss_epoch_hunter";
    newscript->GetAI = &GetAI_boss_epoch_hunter;
    newscript->RegisterSelf();
}

