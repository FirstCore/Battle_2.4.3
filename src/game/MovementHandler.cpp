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

#include "Common.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Opcodes.h"
#include "Log.h"
#include "World.h"
#include "Corpse.h"
#include "Player.h"
#include "MapManager.h"
#include "Transports.h"
#include "BattleGround.h"
#include "WaypointMovementGenerator.h"
#include "InstanceSaveMgr.h"
#include "Language.h"

void WorldSession::HandleMoveWorldportAckOpcode(WorldPacket & /*recv_data*/)
{
    DEBUG_LOG("WORLD: got MSG_MOVE_WORLDPORT_ACK.");
    HandleMoveWorldportAckOpcode();
}

void WorldSession::HandleMoveWorldportAckOpcode()
{
    DEBUG_LOG("WORLD: HandleMoveWorldportAckOpcode.");
    // ignore unexpected far teleports
    if (!GetPlayer()->IsBeingTeleportedFar())
        return;

    // get the teleport destination
    WorldLocation &loc = GetPlayer()->GetTeleportDest();

    // possible errors in the coordinate validity check
    if (!MapManager::IsValidMapCoord(loc))
    {
        sLog.outError("WorldSession::HandleMoveWorldportAckOpcode: player %s (%d) was teleported far to a not valid location. (map:%u, x:%f, y:%f, "
            "z:%f) We port him to his homebind instead..", GetPlayer()->GetName(), GetPlayer()->GetGUIDLow(), loc.GetMapId(), loc.GetPositionX(), loc.GetPositionY(), loc.GetPositionZ());
        // stop teleportation else we would try this again and again in LogoutPlayer...
        GetPlayer()->SetSemaphoreTeleportFar(false);
        // and teleport the player to a valid place
        GetPlayer()->TeleportToHomebind();
        return;
    }

    //reset falltimer at teleport
    GetPlayer()->m_anti_justteleported = true;

    // get the destination map entry, not the current one, this will fix homebind and reset greeting
    MapEntry const* mEntry = sMapStore.LookupEntry(loc.GetMapId());
    InstanceTemplate const* mInstance = objmgr.GetInstanceTemplate(loc.GetMapId());

    // reset instance validity, except if going to an instance inside an instance
    if (GetPlayer()->m_InstanceValid == false && !mInstance)
        GetPlayer()->m_InstanceValid = true;

    GetPlayer()->SetSemaphoreTeleportFar(false);

    Map * oldMap = GetPlayer()->GetMap();
    ASSERT(oldMap);
    if (GetPlayer()->IsInWorld())
    {
        sLog.outCrash("Player is still in world when teleported from map %u! to new map %u", oldMap->GetId(), loc.GetMapId());
        oldMap->Remove(GetPlayer(), false);
    }

    // relocate the player to the teleport destination
    Map * newMap = MapManager::Instance().CreateMap(loc.GetMapId(), GetPlayer(), 0);
    // the CanEnter checks are done in TeleporTo but conditions may change
    // while the player is in transit, for example the map may get full
    if (!newMap || !newMap->CanEnter(GetPlayer()))
    {
        sLog.outError("Map %d could not be created for player %d, porting player to homebind", loc.GetMapId(), GetPlayer()->GetGUIDLow());
        GetPlayer()->TeleportToHomebind();
        return;
    }
    else
        GetPlayer()->Relocate(&loc);

    GetPlayer()->ResetMap();
    GetPlayer()->SetMap(newMap);

    // check this before Map::Add(player), because that will create the instance save!
    bool reset_notify = (GetPlayer()->GetBoundInstance(GetPlayer()->GetMapId(), GetPlayer()->GetDifficulty()) == NULL);

    GetPlayer()->SendInitialPacketsBeforeAddToMap();
    if (!GetPlayer()->GetMap()->Add(GetPlayer()))
    {
        sLog.outError("WORLD: failed to teleport player %s (%d) to map %d because of unknown reason!", GetPlayer()->GetName(), GetPlayer()->GetGUIDLow(), loc.GetMapId());
        GetPlayer()->ResetMap();
        GetPlayer()->SetMap(oldMap);
        GetPlayer()->TeleportToHomebind();
        return;
    }

    // battleground state prepare (in case join to BG), at relogin/tele player not invited
    // only add to bg group and object, if the player was invited (else he entered through command)
    if (GetPlayer()->InBattleGround())
    {
        // cleanup setting if outdated
        if (!mEntry->IsBattleGroundOrArena())
        {
            // We're not in BG
            GetPlayer()->SetBattleGroundId(0);                          // We're not in BG.
            // reset destination bg team
            GetPlayer()->SetBGTeam(0);
        }
        // join to bg case
        else if (BattleGround *bg = GetPlayer()->GetBattleGround())
        {
            if (GetPlayer()->IsInvitedForBattleGroundInstance(GetPlayer()->GetBattleGroundId()))
                bg->AddPlayer(GetPlayer());
        }
    }

    GetPlayer()->SendInitialPacketsAfterAddToMap();

    // flight fast teleport case
    if (GetPlayer()->GetMotionMaster()->GetCurrentMovementGeneratorType() == FLIGHT_MOTION_TYPE)
    {
        if (!GetPlayer()->InBattleGround())
        {
            // short preparations to continue flight
            FlightPathMovementGenerator* flight = (FlightPathMovementGenerator*)(GetPlayer()->GetMotionMaster()->top());
            flight->Initialize(*GetPlayer());
            return;
        }

        // battleground state prepare, stop flight
        GetPlayer()->GetMotionMaster()->MovementExpired();
        GetPlayer()->CleanupAfterTaxiFlight();
    }

    // resurrect character at enter into instance where his corpse exist after add to map
    Corpse *corpse = GetPlayer()->GetCorpse();
    if (corpse && corpse->GetType() != CORPSE_BONES && corpse->GetMapId() == GetPlayer()->GetMapId())
    {
        if (mEntry->IsDungeon())
        {
            GetPlayer()->ResurrectPlayer(0.5f, false);
            GetPlayer()->SpawnCorpseBones();
        }
    }

    if (mEntry->IsRaid() && mInstance)
    {
        if (reset_notify)
        {
            uint32 timeleft = sInstanceSaveManager.GetResetTimeFor(GetPlayer()->GetMapId()) - time(NULL);
            GetPlayer()->SendInstanceResetWarning(GetPlayer()->GetMapId(), timeleft); // greeting at the entrance of the resort raid instance
        }
    }

    // mount allow check
    if (!mEntry->IsMountAllowed())
        GetPlayer()->RemoveSpellsCausingAura(SPELL_AURA_MOUNTED);

    // honorless target
    if (GetPlayer()->pvpInfo.inHostileArea)
        GetPlayer()->CastSpell(GetPlayer(), 2479, true);

    // resummon pet
    if (GetPlayer()->m_temporaryUnsummonedPetNumber)
    {
        Pet* NewPet = new Pet(GetPlayer());
        if (!NewPet->LoadPetFromDB(GetPlayer(), 0, GetPlayer()->m_temporaryUnsummonedPetNumber, true))
            delete NewPet;

        GetPlayer()->m_temporaryUnsummonedPetNumber = 0;
    }

    //lets process all delayed operations on successful teleport
    GetPlayer()->ProcessDelayedOperations();
}

void WorldSession::HandleMoveTeleportAck(WorldPacket& recv_data)
{
    uint64 guid;
    uint32 flags, time;

    recv_data >> guid;
    recv_data >> flags >> time;
    DEBUG_LOG("Guid " UI64FMTD, guid);
    DEBUG_LOG("Flags %u, time %u", flags, time/IN_MILLISECONDS);

    Unit* mover = _player->m_mover;
    Player *plMover = mover->GetTypeId() == TYPEID_PLAYER ? mover->ToPlayer() : NULL;

    if (!plMover || !plMover->IsBeingTeleportedNear())
        return;

    if (guid != plMover->GetGUID())
        return;

    //reset falltimer at teleport
    plMover->m_anti_justteleported = true;

    plMover->SetSemaphoreTeleportNear(false);

    uint32 old_zone = plMover->GetZoneId();

    WorldLocation const& dest = plMover->GetTeleportDest();

    plMover->SetPosition(dest, true);

    uint32 newzone = plMover->GetZoneId();

    plMover->UpdateZone(newzone);

    // new zone
    if (old_zone != newzone)
    {
        // honorless target
        if (plMover->pvpInfo.inHostileArea)
            plMover->CastSpell(plMover, 2479, true);
    }

    // resummon pet
    if (plMover->m_temporaryUnsummonedPetNumber)
    {
        Pet* NewPet = new Pet(plMover);
        if (!NewPet->LoadPetFromDB(plMover, 0, plMover->m_temporaryUnsummonedPetNumber, true))
            delete NewPet;

        plMover->m_temporaryUnsummonedPetNumber = 0;
    }

    //lets process all delayed operations on successful teleport
    plMover->ProcessDelayedOperations();
}

void WorldSession::HandleMovementOpcodes(WorldPacket& recv_data)
{
    uint32 timediff = getMSTime();

    Unit* mover = _player->m_mover;

    ASSERT(mover != NULL);                                  // there must always be a mover

    Player *plMover = mover->GetTypeId() == TYPEID_PLAYER ? mover->ToPlayer() : NULL;

    // ignore, waiting processing in WorldSession::HandleMoveWorldportAckOpcode and WorldSession::HandleMoveTeleportAck
    if (plMover && plMover->IsBeingTeleported())
    {
        plMover->m_anti_justteleported = true;
        recv_data.rpos(recv_data.wpos());                   // prevent warnings spam
        return;
    }

    //get opcode
    uint16 opcode = recv_data.GetOpcode();

    /* extract packet */
    MovementInfo movementInfo;
    recv_data >> movementInfo;
    /*----------------*/

    /*if (recv_data.size() != recv_data.rpos())
    {
        sLog.outError("MovementHandler: player %s (guid %d, account %u) sent a packet (opcode %u) that is %u bytes larger than it should be. Kicked as cheater.", _player->GetName(), _player->GetGUIDLow(), _player->GetSession()->GetAccountId(), opcode, recv_data.size() - recv_data.rpos());
        KickPlayer();
        return;
    }*/

    if (!BlizzLike::IsValidMapCoord(movementInfo.GetPos()->GetPositionX(), movementInfo.GetPos()->GetPositionY(), movementInfo.GetPos()->GetPositionZ(), movementInfo.GetPos()->GetOrientation()))
        return;

    bool updateOrientationOnly = false;
    bool skipAnticheat = false;
    bool useFallingFlag = false;
    bool forcemovement = false;
    bool check_passed = true;

    if (!movementInfo.HasMovementFlag(MOVEFLAG_MOVING) && !mover->HasUnitMovementFlag(MOVEFLAG_MOVING) && !(opcode == MSG_MOVE_FALL_LAND))
    {
        #ifdef MOVEMENT_ANTICHEAT_DEBUG
        sLog.outDetail("Update Orientation Only");
        #endif
        skipAnticheat = true;
        updateOrientationOnly = true;
    }

    //Save movement flags
    mover->SetUnitMovementFlags(movementInfo.GetMovementFlags());

    /* handle special cases */
    if (movementInfo.HasMovementFlag(MOVEFLAG_ONTRANSPORT))
    {
        // transports size limited
        // (also received at zeppelin/lift leave by some reason with t_* as absolute in continent coordinates, can be safely skipped)
        if (movementInfo.GetTransportPos()->GetPositionX() > 60 || movementInfo.GetTransportPos()->GetPositionY() > 60 || movementInfo.GetTransportPos()->GetPositionZ() > 60)
            return;

        if (!BlizzLike::IsValidMapCoord(movementInfo.GetPos()->GetPositionX() + movementInfo.GetTransportPos()->GetPositionX(), movementInfo.GetPos()->GetPositionY() + movementInfo.GetTransportPos()->GetPositionY(),
            movementInfo.GetPos()->GetPositionZ() + movementInfo.GetTransportPos()->GetPositionZ(), movementInfo.GetPos()->GetOrientation() + movementInfo.GetTransportPos()->GetOrientation()))
            return;

        if (plMover && plMover->m_anti_transportGUID == 0 && movementInfo.t_guid != 0)
        {
            // if we boarded a transport, add us to it
            if (!plMover->m_transport)
            {
                float trans_rad = movementInfo.GetTransportPos()->GetPositionX()*movementInfo.GetTransportPos()->GetPositionX() + movementInfo.GetTransportPos()->GetPositionY()*movementInfo.GetTransportPos()->GetPositionY() + movementInfo.GetTransportPos()->GetPositionZ()*movementInfo.GetTransportPos()->GetPositionZ();
                if (trans_rad > 3600.0f) // transport radius = 60 yards //cheater with on_transport_flag
                {
                    return;
                }
                // elevators also cause the client to send MOVEFLAG_ONTRANSPORT - just unmount if the guid can be found in the transport list
                for (MapManager::TransportSet::iterator iter = MapManager::Instance().m_Transports.begin(); iter != MapManager::Instance().m_Transports.end(); ++iter)
                {
                    if ((*iter)->GetGUID() == movementInfo.t_guid)
                    {
                        // unmount before boarding
                        plMover->RemoveSpellsCausingAura(SPELL_AURA_MOUNTED);

                        plMover->m_transport = (*iter);
                        (*iter)->AddPassenger(plMover);
                        break;
                    }
                }
            }

            GameObject *obj = HashMapHolder<GameObject>::Find(movementInfo.t_guid);
            if (obj)
                plMover->m_anti_transportGUID = obj->GetDBTableGUIDLow();
            else
                plMover->m_anti_transportGUID = GUID_LOPART(movementInfo.t_guid);

            #ifdef MOVEMENT_ANTICHEAT_DEBUG
            sLog.outDetail("On Transport %d", plMover->m_anti_transportGUID ? plMover->m_anti_transportGUID : 0);
            #endif
        }
    }
    else if (plMover && plMover->m_transport)               // if we were on a transport, leave
    {
        #ifdef MOVEMENT_ANTICHEAT_DEBUG
        sLog.outDetail("Left the Transport %d", plMover->m_anti_transportGUID ? plMover->m_anti_transportGUID : 0);
        #endif

        plMover->m_transport->RemovePassenger(plMover);
        plMover->m_transport = NULL;
        plMover->m_anti_transportGUID = 0;
        movementInfo.ClearTransportData();
    }

    else if (plMover && plMover->m_anti_transportGUID != 0)
    {
        #ifdef MOVEMENT_ANTICHEAT_DEBUG
        sLog.outDetail("No more Transport %d", plMover->m_anti_transportGUID ? plMover->m_anti_transportGUID : 0);
        #endif
        plMover->m_anti_transportGUID = 0;
    }

    if (plMover && World::GetEnableMvAnticheat() && GetPlayer()->GetSession()->GetSecurity() <= World::GetMvAnticheatGmLevel())
    {
        // fall damage generation (ignore in flight case that can be triggered also at lags in moment teleportation to another map).
        if (opcode == MSG_MOVE_FALL_LAND && !plMover->isInFlight())
        {
            if (!plMover->m_anti_ontaxipath && !plMover->m_anti_justteleported && !plMover->m_anti_flymounted)
                plMover->HandleFallDamage(movementInfo);

            // Fix Blink / Shadowstep
            if (plMover->m_anti_justteleported)
                skipAnticheat = true;

            // Fix KnockBack
            if (plMover->m_anti_isknockedback)
                forcemovement = true;

            plMover->m_anti_justteleported = false;
            plMover->m_anti_ontaxipath = false;
            plMover->m_anti_wasflymounted = false;
            plMover->m_anti_justjumped = 0;
            plMover->m_anti_jumpbase = 0;
            plMover->m_anti_isjumping = false;
            plMover->m_anti_isknockedback = false;
        }
        else if (opcode == MSG_MOVE_START_SWIM)
            plMover->m_anti_isjumping = false;
        else if (opcode == MSG_MOVE_JUMP)
            plMover->m_anti_isjumping = true;

        if (plMover->m_anti_isknockedback)
            forcemovement = true;

        if (movementInfo.HasMovementFlag(MOVEFLAG_SWIMMING) != plMover->IsInWater())
        {
            // now client not include swimming flag in case jumping under water
            plMover->SetInWater(!plMover->IsInWater() || plMover->GetBaseMap()->IsUnderWater(movementInfo.GetPos()->GetPositionX(), movementInfo.GetPos()->GetPositionY(), movementInfo.GetPos()->GetPositionZ()));
            plMover->m_anti_justjumped = 0;
        }
        /*----------------------*/

        #ifdef MOVEMENT_ANTICHEAT_DEBUG
        sLog.outBasic("%s newcoord: tm:%d ftm:%d | %f, %f, %fo(%f) [%X][%s]| transport: %f, %f, %fo(%f)", plMover->GetName(), movementInfo.time, movementInfo.GetFallTime(), movementInfo.GetPos()->GetPositionX(), movementInfo.GetPos()->GetPositionY(), movementInfo.GetPos()->GetPositionZ(), movementInfo.GetPos()->GetOrientation(), movementInfo.GetMovementFlags(), LookupOpcodeName(opcode), movementInfo.GetTransportPos()->GetPositionX(), movementInfo.GetTransportPos()->GetPositionY(), movementInfo.GetTransportPos()->GetPositionZ(), movementInfo.GetTransportPos()->GetOrientation());
        sLog.outBasic("Transport: %d |  tguid: %d - %d", plMover->m_anti_transportGUID, GUID_LOPART(movementInfo.t_guid), GUID_HIPART(movementInfo.t_guid));
        #endif

        //---- anti-cheat features -->>>
        if (!plMover->IsInWater() && updateOrientationOnly && plMover->m_anti_transportGUID == 0)
        {
            if ((abs(plMover->GetPositionX() - movementInfo.GetPos()->GetPositionX()) > 0.1f) ||
                (abs(plMover->GetPositionY() - movementInfo.GetPos()->GetPositionY()) > 0.1f) ||
                (abs(plMover->GetPositionZ() - movementInfo.GetPos()->GetPositionZ()) > 0.1f))
            {
                #ifdef MOVEMENT_ANTICHEAT_DEBUG
                sLog.outError("Passiv Movement: dx=%f, dy=%f, dz=%f",
                    abs(plMover->GetPositionX() - movementInfo.GetPos()->GetPositionX()),
                    abs(plMover->GetPositionY() - movementInfo.GetPos()->GetPositionY()),
                    abs(plMover->GetPositionZ() - movementInfo.GetPos()->GetPositionZ()));
                #endif
                plMover->m_anti_lastcheat = "Passiv Movement Hack";
                check_passed = false;
            }
        }

        if ((plMover->m_anti_transportGUID == 0) && !plMover->m_anti_ontaxipath && !skipAnticheat)
        {
            UnitMoveType move_type;

            if (movementInfo.HasMovementFlag(MOVEFLAG_FLYING))
                move_type = movementInfo.HasMovementFlag(MOVEFLAG_BACKWARD) ? MOVE_FLIGHT_BACK : MOVE_FLIGHT;
            else if (movementInfo.HasMovementFlag(MOVEFLAG_SWIMMING))
                move_type = movementInfo.HasMovementFlag(MOVEFLAG_BACKWARD) ? MOVE_SWIM_BACK : MOVE_SWIM;
            else if (movementInfo.HasMovementFlag(MOVEFLAG_WALK_MODE))
                move_type = MOVE_WALK;
            else    //hmm... in first time after login player has MOVE_SWIMBACK instead MOVE_WALKBACK
                move_type = movementInfo.HasMovementFlag(MOVEFLAG_BACKWARD) ? MOVE_SWIM_BACK : MOVE_RUN;

            float allowed_delta = 0;
            float current_speed = plMover->GetSpeed(move_type);

            float delta_x = plMover->GetPositionX() - movementInfo.GetPos()->GetPositionX();
            float delta_y = plMover->GetPositionY() - movementInfo.GetPos()->GetPositionY();
            float delta_z = plMover->GetPositionZ() - movementInfo.GetPos()->GetPositionZ();
            float real_delta = delta_x * delta_x + delta_y * delta_y;

            float time_delta = movementInfo.time - plMover->m_anti_lastmovetime;
            if (time_delta > 0)
                plMover->m_anti_lastmovetime = movementInfo.time;
            else
                time_delta = 0;

            time_delta = (time_delta < 1500) ? time_delta / 1000 : 1.5f; // normalize time - 1.5 second allowed for heavy loaded server

            float tg_z = -99999; // tangens
            if (!(movementInfo.GetMovementFlags() & (MOVEFLAG_FLYING | MOVEFLAG_SWIMMING)))
                tg_z = (real_delta != 0) ? (delta_z * delta_z / real_delta) : tg_z;

            if (current_speed < plMover->m_anti_last_hspeed)
            {
                allowed_delta = plMover->m_anti_last_hspeed;
                if (plMover->m_anti_lastspeed_changetime == 0)
                    plMover->m_anti_lastspeed_changetime = movementInfo.time + (uint32)floor(((plMover->m_anti_last_hspeed / current_speed) * 1000)) + 100; //100ms above for random fluctuating =)))
            }
            else
                allowed_delta = current_speed;

            allowed_delta = allowed_delta * time_delta;
            allowed_delta = allowed_delta * allowed_delta + 2;

            // Anti-Gravitation
            float JumpHeight = ((plMover->m_anti_jumpbase > 0) ? plMover->m_anti_jumpbase : 0) - movementInfo.GetPos()->GetPositionZ();
            if ((plMover->m_anti_jumpbase != 0)
                        && (plMover->m_anti_justjumped > 0)
                        && (!plMover->m_anti_justteleported)
                        && !(movementInfo.HasMovementFlag(MOVEFLAG_SWIMMING) || movementInfo.HasMovementFlag(MOVEFLAG_FLYING)
                            || movementInfo.HasMovementFlag(MOVEFLAG_FLYING2))
                        && (JumpHeight < GetPlayer()->m_anti_last_vspeed))
            {
                #ifdef MOVEMENT_ANTICHEAT_DEBUG
                sLog.outError("Movement Anticheat: %s is GraviJump exception. anti_jumpbase=%f, anti_justjumped=%u, JumpHeight=%f, last_vspeed=%f",
                    plMover->GetName(), plMover->m_anti_jumpbase, plMover->m_anti_justjumped, JumpHeight, plMover->m_anti_last_vspeed);
                #endif
                plMover->m_anti_lastcheat = "GraviJump Hack";
                check_passed = false;
            }

            // Anti-Jumphack
            if (opcode == MSG_MOVE_JUMP && !plMover->IsInWater())
            {
                if (plMover->m_anti_justjumped == 0)
                {
                    plMover->m_anti_justjumped += 1;
                    plMover->m_anti_jumpbase = movementInfo.GetPos()->GetPositionZ();
                }
                else
                {
                    #ifdef MOVEMENT_ANTICHEAT_DEBUG
                    sLog.outError("Movement Anticheat: %s is Jumphack exception. count=%u",plMover->GetName(), plMover->m_anti_justjumped);
                    #endif
                    plMover->m_anti_justjumped = 0;
                    plMover->m_anti_lastcheat = "Jump Hack";
                    check_passed = false; //don't process new jump packet
                }
            }
            else if (plMover->IsInWater())
                plMover->m_anti_justjumped = 0;

            // Anti-Speedhack
            // Disabled to revert: if ((real_delta > allowed_delta) && (delta_z < (plMover->m_anti_last_vspeed * time_delta) || delta_z < 1))
            if ((real_delta > allowed_delta) && (delta_z < 1))
            {
                #ifdef MOVEMENT_ANTICHEAT_DEBUG
                sLog.outError("Movement Anticheat: %s is speed exception. real_delta=%f, allowed_delta=%f, delta_z=%f, last_vspeed=%f", plMover->GetName(), real_delta, allowed_delta, delta_z, plMover->m_anti_last_vspeed * time_delta);
                #endif
                plMover->m_anti_lastcheat = "Speed Hack";
                check_passed = false;
            }

            // Anti-Teleport
            // Disabled to revert: if ((real_delta > allowed_delta) && (real_delta > (time_delta * 100)))
            if ((real_delta > 4900.0f) && !(real_delta < allowed_delta))
            {
                #ifdef MOVEMENT_ANTICHEAT_DEBUG
                sLog.outError("Movement Anticheat: %s is teleport exception. real_delta=%f, allowed_delta=%f, min_delta=%f ", plMover->GetName(), real_delta, allowed_delta, time_delta * 100);
                #endif
                plMover->m_anti_lastcheat = "Teleport Hack";
                check_passed = false;
            }

            if (movementInfo.time > plMover->m_anti_lastspeed_changetime)
            {
                plMover->m_anti_last_hspeed = current_speed; // store current speed
                plMover->m_anti_last_vspeed = -3.2f; // original value: -2.3f
                plMover->m_anti_lastspeed_changetime = 0;
            }

            // Anti-Wallhack
            // Known issues: jump+up, and walking up with low delta_z (one and only way to make it right is to calculate the delta_z of the terrain)
            // Disabled to revert: if (!plMover->m_anti_isjumping && (tg_z > 1.6f) && (delta_z < (plMover->m_anti_last_vspeed * time_delta)))
            if (!plMover->m_anti_isjumping && (tg_z > 1.56f) && (delta_z < plMover->m_anti_last_vspeed))
            {
                #ifdef MOVEMENT_ANTICHEAT_DEBUG
                sLog.outError("Movement Anticheat: %s is a wall-climb cheater. tg_z=%f, delta_z=%f, last_vspeed=%f", plMover->GetName(), tg_z, delta_z, plMover->m_anti_last_vspeed * time_delta);
                #endif
                plMover->m_anti_lastcheat = "Wall-climbing Hack";
                check_passed = false;
            }

            // Anti-Flyhack
            if (((movementInfo.GetMovementFlags() & (MOVEFLAG_CAN_FLY | MOVEFLAG_FLYING | MOVEFLAG_FLYING2)) != 0) && !plMover->IsInWater() && !(plMover->HasAuraType(SPELL_AURA_FLY) || plMover->HasAuraType(SPELL_AURA_MOD_FLIGHT_SPEED_MOUNTED)))
            {
                #ifdef MOVEMENT_ANTICHEAT_DEBUG
                sLog.outError("Movement Anticheat: %s is a fly cheater. {SPELL_AURA_FLY=[%X]} {SPELL_AURA_MOD_FLIGHT_SPEED_MOUNTED=[%X]} {SPELL_AURA_MOD_FLIGHT_SPEED_STACKING=[%X]} {SPELL_AURA_MOD_FLIGHT_SPEED_MOUNTED_STACKING=[%X]} {SPELL_AURA_MOD_FLIGHT_SPEED_MOUNTED_NOT_STACKING=[%X]}",
                   plMover->GetName(),
                   plMover->HasAuraType(SPELL_AURA_FLY), plMover->HasAuraType(SPELL_AURA_MOD_FLIGHT_SPEED_MOUNTED),
                   plMover->HasAuraType(SPELL_AURA_MOD_FLIGHT_SPEED_STACKING), plMover->HasAuraType(SPELL_AURA_MOD_FLIGHT_SPEED_MOUNTED_STACKING),
                   plMover->HasAuraType(SPELL_AURA_MOD_FLIGHT_SPEED_MOUNTED_NOT_STACKING));
                #endif
                plMover->m_anti_lastcheat = "Fly Hack";
                useFallingFlag = true;
                check_passed = false;
            }

            // Anti-Waterwalk
            if (movementInfo.HasMovementFlag(MOVEFLAG_WATERWALKING) && !(plMover->HasAuraType(SPELL_AURA_WATER_WALK) | plMover->HasAuraType(SPELL_AURA_GHOST)))
            {
                #ifdef MOVEMENT_ANTICHEAT_DEBUG
                sLog.outError("Movement Anticheat: %s is a water-walk cheater. MovementFlags=[%X], SPELL_AURA_WATER_WALK=[%X]", plMover->GetName(), movementInfo.GetMovementFlags(), plMover->HasAuraType(SPELL_AURA_WATER_WALK));
                #endif
                plMover->m_anti_lastcheat = "Water Walk Hack";
                check_passed = false;
            }

            // Anti-Featherfall
            if (movementInfo.HasMovementFlag(MOVEFLAG_SAFE_FALL) && !(plMover->HasAuraType(SPELL_AURA_FEATHER_FALL)))
            {
                #ifdef MOVEMENT_ANTICHEAT_DEBUG
                sLog.outError("Movement Anticheat: %s is a featherfall-fall cheater. MovementFlags=[%X], SPELL_AURA_FEATHER_FALL=[%X]", plMover->GetName(), movementInfo.GetMovementFlags(), plMover->HasAuraType(SPELL_AURA_FEATHER_FALL));
                #endif
                plMover->m_anti_lastcheat = "Feather Fall Hack";
                check_passed = false;
            }

            // Anti-TeleportToPlane
            if (movementInfo.GetPos()->GetPositionZ() < 0.0001f && movementInfo.GetPos()->GetPositionZ() > -0.0001f && ((movementInfo.GetMovementFlags() & (MOVEFLAG_SWIMMING | MOVEFLAG_CAN_FLY | MOVEFLAG_FLYING | MOVEFLAG_FLYING2)) == 0))
            {
                // Prevent using TeleportToPlane.
                Map *map = plMover->GetMap();
                if (map)
                {
                    float plane_z = map->GetHeight(movementInfo.GetPos()->GetPositionX(), movementInfo.GetPos()->GetPositionY(), MAX_HEIGHT) - movementInfo.GetPos()->GetPositionZ();
                    plane_z = (plane_z < -500.0f) ? 0 : plane_z; // check holes in height map
                    if (plane_z > 0.1f || plane_z < -0.1f)
                    {
                        #ifdef MOVEMENT_ANTICHEAT_DEBUG
                        sLog.outError("Movement Anticheat: %s uses teleport to plane. plane_z: %f ", plMover->GetName(), plane_z);
                        #endif
                        plMover->m_anti_lastcheat = "Teleport to Plane Hack";
                        check_passed = false;
                        plMover->GetSession()->KickPlayer();
                        sWorld.SendGMText(LANG_GM_AC_KICK_ANNOUNCE, plMover->GetName());
                    }
                }
            }
        }
        else if (movementInfo.HasMovementFlag(MOVEFLAG_ONTRANSPORT))
        {
            // Anti-Wrap
            if (plMover->m_transport)
            {
                float trans_rad = movementInfo.GetTransportPos()->GetPositionX() * movementInfo.GetTransportPos()->GetPositionX() + movementInfo.GetTransportPos()->GetPositionY() * movementInfo.GetTransportPos()->GetPositionY() + movementInfo.GetTransportPos()->GetPositionZ() * movementInfo.GetTransportPos()->GetPositionZ();
                if (trans_rad > 3600.0f)
                    check_passed = false;
            }
            else
            {
                if (GameObjectData const* go_data = objmgr.GetGOData(plMover->m_anti_transportGUID))
                {
                    #ifdef MOVEMENT_ANTICHEAT_DEBUG
                    sLog.outError("Movement Anticheat: %s on some transport. xyz: %f, %f, %f", plMover->GetName(), go_data->posX, go_data->posY, go_data->posZ);
                    #endif

                    int mapid = go_data->mapid;
                    if (plMover->GetMapId() != mapid)
                    {
                        check_passed = false;
                    }
                    else if (mapid != 369)
                    {
                        float delta_gox = go_data->posX - movementInfo.GetPos()->GetPositionX();
                        float delta_goy = go_data->posY - movementInfo.GetPos()->GetPositionY();
                        float delta_go = delta_gox*delta_gox + delta_goy*delta_goy;
                        if (delta_go > 3600.0f)
                            check_passed = false;
                    }
                }
                else
                {
                    #ifdef MOVEMENT_ANTICHEAT_DEBUG
                    sLog.outError("Movement Anticheat: %s on undefined transport.", plMover->GetName());
                    #endif
                    check_passed = false;
                }
            }

            if (!check_passed)
            {
                plMover->m_anti_lastcheat = "Transport";
                if (plMover->m_transport)
                {
                    plMover->m_transport->RemovePassenger(plMover);
                    plMover->m_transport = NULL;
                }
                movementInfo.ClearTransportData();
                plMover->m_anti_transportGUID = 0;
            }
        }
    }

    if (plMover)                                            // nothing is charmed, or player charmed
    {
        if (check_passed || forcemovement)
        {
            /* process position-change */
            recv_data.put<uint32>(5, getMSTime());                  // offset flags(4) + unk(1)
            WorldPacket data(opcode, mover->GetPackGUID().size() + recv_data.size());
            data << mover->GetPackGUID();
            data.append(recv_data.contents(), recv_data.size());
            mover->SendMessageToSet(&data, false);
            if (updateOrientationOnly)
                plMover->SetPosition(plMover->GetPositionX(), plMover->GetPositionY(), plMover->GetPositionZ(), movementInfo.GetPos()->GetOrientation());
            else
                plMover->SetPosition(movementInfo.GetPos()->GetPositionX(), movementInfo.GetPos()->GetPositionY(), movementInfo.GetPos()->GetPositionZ(), movementInfo.GetPos()->GetOrientation());

            plMover->m_movementInfo = movementInfo;

            if (opcode == MSG_MOVE_FALL_LAND || plMover->m_lastFallTime > movementInfo.GetFallTime() || plMover->m_lastFallZ < movementInfo.GetPos()->GetPositionZ())
                plMover->SetFallInformation(movementInfo.GetFallTime(), movementInfo.GetPos()->GetPositionZ());
            // we should add the check only for class hunter
            if (plMover->isMovingOrTurning())
                plMover->RemoveSpellsCausingAura(SPELL_AURA_FEIGN_DEATH);

            if (movementInfo.GetPos()->GetPositionZ() < -500.0f)
                plMover->HandleFallUnderMap();

            if (plMover->m_anti_alarmcount > 0)
            {
                sWorld.SendGMText(LANG_GM_AC_ANNOUNCE, plMover->GetName(), plMover->m_anti_lastcheat.c_str());
            }

            plMover->m_anti_alarmcount = 0;
        }
        else
        {
            plMover->m_anti_alarmcount++;
            WorldPacket data;
            // Temporary disabled maybee cause fall dmg on dismount in air
            /* if (useFallingFlag)
                plMover->SetUnitMovementFlags(MOVEFLAG_FALLING);
            else
                plMover->SetUnitMovementFlags(MOVEFLAG_NONE);
            */
            plMover->SetUnitMovementFlags(MOVEFLAG_NONE);
            plMover->BuildTeleportAckMsg(&data, plMover->GetPositionX(), plMover->GetPositionY(), plMover->GetPositionZ(), plMover->GetOrientation());
            plMover->GetSession()->SendPacket(&data);
            plMover->BuildHeartBeatMsg(&data);
            plMover->SendMessageToSet(&data, true);
        }
    }
    else
        if (getMSTimeDiff(timediff, getMSTime()) > 20)
            sLog.outError("Anticheat Process Time: %ums : %s [%x]", getMSTimeDiff(timediff, getMSTime()), LookupOpcodeName(opcode), movementInfo.GetMovementFlags());
}

void WorldSession::HandleForceSpeedChangeAck(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Recvd CMSG_SPEED_CHANGE_ACK");
    /* extract packet */
    ObjectGuid guid;
    uint32 unk1;
    MovementInfo movementInfo;
    float newspeed;

    recv_data >> guid;
    recv_data >> unk1;                           // counter or moveEvent
    recv_data >> movementInfo;
    recv_data >> newspeed;

    // now can skip not our packet
    if (GetPlayer()->GetGUID() != guid.GetRawValue())
        return;
    /*----------------*/

    // Save movement flags
    GetPlayer()->SetUnitMovementFlags(movementInfo.GetMovementFlags());

    #ifdef MOVEMENT_ANTICHEAT_DEBUG
    sLog.outBasic("%s CMSG_SPEED_CHANGE_ACK: tm:%d ftm:%d | %f, %f, %fo(%f) [%X]", GetPlayer()->GetName(), movementInfo.time, movementInfo.GetFallTime(), movementInfo.GetPos()->GetPositionX(), movementInfo.GetPos()->GetPositionY(), movementInfo.GetPos()->GetPositionZ(), movementInfo.GetPos()->GetOrientation(), movementInfo.GetMovementFlags());
    sLog.outBasic("%s CMSG_SPEED_CHANGE_ACK additional: vspeed:%f, hspeed:%f, xdir:%f ydir:%f", GetPlayer()->GetName(), movementInfo.j_velocity, movementInfo.j_xyspeed, movementInfo.j_sinAngle, movementInfo.j_cosAngle);
    #endif

    // skip not personal message;
    GetPlayer()->m_movementInfo = movementInfo;
    GetPlayer()->m_anti_last_hspeed = movementInfo.j_xyspeed;
    GetPlayer()->m_anti_last_vspeed = movementInfo.j_velocity < 3.2f ? movementInfo.j_xyspeed - 1.0f : 3.2f;
    GetPlayer()->m_anti_lastspeed_changetime = movementInfo.time + 1750;

    // client ACK send one packet for mounted/run case and need skip all except last from its
    // in other cases anti-cheat check can be fail in false case
    UnitMoveType move_type;
    UnitMoveType force_move_type;

    //static char const* move_type_name[MAX_MOVE_TYPE] = {"Walk", "Run", "RunBack", "Swim", "SwimBack", "TurnRate", "Flight", "FlightBack"};

    uint16 opcode = recv_data.GetOpcode();
    switch(opcode)
    {
        case CMSG_FORCE_WALK_SPEED_CHANGE_ACK:          move_type = MOVE_WALK;          force_move_type = MOVE_WALK;        break;
        case CMSG_FORCE_RUN_SPEED_CHANGE_ACK:           move_type = MOVE_RUN;           force_move_type = MOVE_RUN;         break;
        case CMSG_FORCE_RUN_BACK_SPEED_CHANGE_ACK:      move_type = MOVE_RUN_BACK;      force_move_type = MOVE_RUN_BACK;    break;
        case CMSG_FORCE_SWIM_SPEED_CHANGE_ACK:          move_type = MOVE_SWIM;          force_move_type = MOVE_SWIM;        break;
        case CMSG_FORCE_SWIM_BACK_SPEED_CHANGE_ACK:     move_type = MOVE_SWIM_BACK;     force_move_type = MOVE_SWIM_BACK;   break;
        case CMSG_FORCE_TURN_RATE_CHANGE_ACK:           move_type = MOVE_TURN_RATE;     force_move_type = MOVE_TURN_RATE;   break;
        case CMSG_FORCE_FLIGHT_SPEED_CHANGE_ACK:        move_type = MOVE_FLIGHT;        force_move_type = MOVE_FLIGHT;      break;
        case CMSG_FORCE_FLIGHT_BACK_SPEED_CHANGE_ACK:   move_type = MOVE_FLIGHT_BACK;   force_move_type = MOVE_FLIGHT_BACK; break;
        default:
            sLog.outError("WorldSession::HandleForceSpeedChangeAck: Unknown move type opcode: %u", opcode);
            return;
    }

    // skip all forced speed changes except last and unexpected
    // in run/mounted case used one ACK and it must be skipped.m_forced_speed_changes[MOVE_RUN] store both.
    if (GetPlayer()->m_forced_speed_changes[force_move_type] > 0)
    {
        --GetPlayer()->m_forced_speed_changes[force_move_type];
        if (GetPlayer()->m_forced_speed_changes[force_move_type] > 0)
            return;
    }
}

void WorldSession::HandleSetActiveMoverOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Recvd CMSG_SET_ACTIVE_MOVER");

    uint64 guid;
    recv_data >> guid;

    if (GetPlayer()->IsInWorld())
    {
        if (Unit* mover = ObjectAccessor::GetUnit(*GetPlayer(), guid))
            GetPlayer()->SetMover(mover);
    }
    else
    {
        sLog.outError("HandleSetActiveMoverOpcode: incorrect mover guid: mover is " UI64FMTD " and should be " UI64FMTD, guid, _player->m_mover->GetGUID());
        GetPlayer()->SetMover(GetPlayer());
    }
}

void WorldSession::HandleMoveNotActiveMoverOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Recvd CMSG_MOVE_NOT_ACTIVE_MOVER");
    recv_data.hexlike();

    uint64 old_mover_guid;
    MovementInfo mi;

    recv_data.readPackGUID(old_mover_guid);
    recv_data >> mi;

    if (!old_mover_guid)
        return;

    GetPlayer()->m_movementInfo = mi;
}

void WorldSession::HandleMountSpecialAnimOpcode(WorldPacket& /*recvdata*/)
{
    DEBUG_LOG("WORLD: Recvd CMSG_MOUNTSPECIAL_ANIM");

    WorldPacket data(SMSG_MOUNTSPECIAL_ANIM, 8);
    data << uint64(GetPlayer()->GetGUID());

    GetPlayer()->SendMessageToSet(&data, false);
}

void WorldSession::HandleMoveKnockBackAck(WorldPacket& recv_data)
{
    // Currently not used but maybe use later for recheck final player position
    // (must be at call same as into "recv_data >> x >> y >> z >> orientation;"
    DEBUG_LOG("CMSG_MOVE_KNOCK_BACK_ACK");

    MovementInfo movementInfo;
    uint64 guid;
    uint32 unk1;

    recv_data >> guid;                                      // guid
    recv_data >> unk1;                                      // unk
    recv_data >> movementInfo;

    if (GetPlayer()->GetGUID() != guid)
        return;

    // Save movement flags
    GetPlayer()->SetUnitMovementFlags(movementInfo.GetMovementFlags());
    GetPlayer()->m_anti_isknockedback = true;
    //Todo: Store the final Position and check it with the Position of MSG_MOVE_FALL_LAND in HandleMovementOpcodes

    #ifdef MOVEMENT_ANTICHEAT_DEBUG
    sLog.outBasic("%s CMSG_MOVE_KNOCK_BACK_ACK: tm:%d ftm:%d | %f, %f, %fo(%f) [%X]", GetPlayer()->GetName(), movementInfo.time, movementInfo.GetFallTime(), movementInfo.GetPos()->GetPositionX(), movementInfo.GetPos()->GetPositionY(), movementInfo.GetPos()->GetPositionZ(), movementInfo.GetPos()->GetOrientation(), movementInfo.GetMovementFlags());
    sLog.outBasic("%s CMSG_MOVE_KNOCK_BACK_ACK additional: vspeed:%f, hspeed:%f, xdir:%f ydir:%f", GetPlayer()->GetName(), movementInfo.j_velocity, movementInfo.j_xyspeed, movementInfo.j_sinAngle, movementInfo.j_cosAngle);
    #endif

    // skip not personal message;
    GetPlayer()->m_movementInfo = movementInfo;
    GetPlayer()->m_anti_last_hspeed = movementInfo.j_xyspeed;
    GetPlayer()->m_anti_last_vspeed = movementInfo.j_velocity < 3.2f ? movementInfo.j_xyspeed - 1.0f : 3.2f;
    GetPlayer()->m_anti_lastspeed_changetime = movementInfo.time + 1750;
}

void WorldSession::HandleMoveFlyModeChangeAckOpcode(WorldPacket& recv_data)
{
    // fly mode on/off
    DEBUG_LOG("WORLD: CMSG_MOVE_SET_CAN_FLY_ACK");
    //recv_data.hexlike();

    MovementInfo movementInfo;

    recv_data.read_skip<uint64>();                          // guid
    recv_data.read_skip<uint32>();                          // unk
    recv_data >> movementInfo;
    recv_data.read_skip<uint32>();                          // unk2

    // Movement Anticheat
    if (movementInfo.HasMovementFlag(MOVEFLAG_FLYING))
    {
        GetPlayer()->m_anti_flymounted = true;
    }
    else
    {
        GetPlayer()->m_anti_flymounted = false;
        GetPlayer()->m_anti_wasflymounted = true;
    }
    //<<< end Movement Anticheat

    GetPlayer()->SetUnitMovementFlags(movementInfo.GetMovementFlags());
}

void WorldSession::HandleMoveHoverAck(WorldPacket& recv_data)
{
    DEBUG_LOG("CMSG_MOVE_HOVER_ACK");

    uint64 guid;                                            // guid - unused
    recv_data.readPackGUID(guid);

    recv_data.read_skip<uint32>();                          // unk

    MovementInfo movementInfo;
    recv_data >> movementInfo;

    recv_data.read_skip<uint32>();                          // unk2
}

void WorldSession::HandleMoveWaterWalkAck(WorldPacket& recv_data)
{
    DEBUG_LOG("CMSG_MOVE_WATER_WALK_ACK");

    uint64 guid;                                            // guid - unused
    recv_data.readPackGUID(guid);

    recv_data.read_skip<uint32>();                          // unk

    MovementInfo movementInfo;
    recv_data >> movementInfo;
    
    recv_data.read_skip<uint32>();                          // unk2
}

void WorldSession::HandleSummonResponseOpcode(WorldPacket& recv_data)
{
    if (!GetPlayer()->isAlive() || GetPlayer()->isInCombat())
        return;

    uint64 summoner_guid;
    bool agree;
    recv_data >> summoner_guid;
    recv_data >> agree;

    GetPlayer()->SummonIfPossible(agree);
}

