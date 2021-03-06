/*
   Copyright (c) 2011, The Mineserver Project
   All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:
  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
  * Neither the name of the The Mineserver Project nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdlib.h>
#include <sys/types.h>
#ifdef WIN32
  #include <conio.h>
  #include <winsock2.h>
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <string.h>
  #include <netdb.h>
  #include <netinet/tcp.h>
#endif

#include <fcntl.h>
#include <cstdio>
#include <deque>
#include <iostream>
#include <event.h>
#include <fstream>
#include <ctime>
#include <cmath>
#include <vector>
#include <zlib.h>
#include <stdint.h>
#include <functional>

#include "chat.h"
#include "config.h"
#include "constants.h"
#include "furnaceManager.h"
#include "inventory.h"
#include "logger.h"
#include "map.h"
#include "mineserver.h"
#include "nbt.h"
#include "packets.h"
#include "physics.h"
#include "plugin.h"
#include "sockets.h"
#include "tools.h"
#include "user.h"
#include "blocks/basic.h"
#include "blocks/note.h"

#ifdef WIN32
    #define M_PI 3.141592653589793238462643
#endif
#define DEGREES_TO_RADIANS(x) ((x) / 180.0 * M_PI)
#define RADIANS_TO_DEGREES(x) ((x) / M_PI * 180.0)

//#define _DEBUG

void PacketHandler::init()
{
  packets[PACKET_KEEP_ALIVE]               = Packets(0, &PacketHandler::keep_alive);
  packets[PACKET_LOGIN_REQUEST]            = Packets(PACKET_VARIABLE_LEN, &PacketHandler::login_request);
  packets[PACKET_HANDSHAKE]                = Packets(PACKET_VARIABLE_LEN, &PacketHandler::handshake);
  packets[PACKET_CHAT_MESSAGE]             = Packets(PACKET_VARIABLE_LEN, &PacketHandler::chat_message);
  packets[PACKET_USE_ENTITY]               = Packets( 9, &PacketHandler::use_entity);
  packets[PACKET_PLAYER]                   = Packets( 1, &PacketHandler::player);
  packets[PACKET_PLAYER_POSITION]          = Packets(33, &PacketHandler::player_position);
  packets[PACKET_PLAYER_LOOK]              = Packets( 9, &PacketHandler::player_look);
  packets[PACKET_PLAYER_POSITION_AND_LOOK] = Packets(41, &PacketHandler::player_position_and_look);
  packets[PACKET_PLAYER_DIGGING]           = Packets(11, &PacketHandler::player_digging);
  packets[PACKET_PLAYER_BLOCK_PLACEMENT]   = Packets(PACKET_VARIABLE_LEN, &PacketHandler::player_block_placement);
  packets[PACKET_HOLDING_CHANGE]           = Packets( 2, &PacketHandler::holding_change);
  packets[PACKET_ARM_ANIMATION]            = Packets( 5, &PacketHandler::arm_animation);
  packets[PACKET_PICKUP_SPAWN]             = Packets(22, &PacketHandler::pickup_spawn);
  packets[PACKET_DISCONNECT]               = Packets(PACKET_VARIABLE_LEN, &PacketHandler::disconnect);
  packets[PACKET_RESPAWN]                  = Packets( 0, &PacketHandler::respawn);
  packets[PACKET_INVENTORY_CHANGE]         = Packets(PACKET_VARIABLE_LEN, &PacketHandler::inventory_change);
  packets[PACKET_INVENTORY_CLOSE]          = Packets(1, &PacketHandler::inventory_close);
  packets[PACKET_SIGN]                     = Packets(PACKET_VARIABLE_LEN, &PacketHandler::change_sign); 
  packets[PACKET_TRANSACTION]              = Packets(4, &PacketHandler::inventory_transaction); 
  packets[PACKET_ENTITY_CROUCH]            = Packets(5, &PacketHandler::entity_crouch); 

  
}


int PacketHandler::entity_crouch(User *user)
{
  int32_t EID;
  int8_t action;

  user->buffer >> EID >> action;

  //ToDo: inform other players
  //Mineserver::get()->logger()->log(LogType::LOG_INFO, "Packets", "Entity action: EID: " + dtos(EID) +" Action: " +dtos(action));

  user->buffer.removePacket();
  return PACKET_OK;
}

int PacketHandler::change_sign(User *user)
{
  if(!user->buffer.haveData(16))
    return PACKET_NEED_MORE_DATA;
  int32_t x,z;
  int16_t y;
  std::string strings1,strings2,strings3,strings4;

  user->buffer >> x >> y >> z;

  if(!user->buffer.haveData(8))
    return PACKET_NEED_MORE_DATA;
  user->buffer >> strings1;
  if(!user->buffer.haveData(6))
    return PACKET_NEED_MORE_DATA;
  user->buffer >> strings2;
  if(!user->buffer.haveData(4))
    return PACKET_NEED_MORE_DATA;
  user->buffer >> strings3;
  if(!user->buffer.haveData(2))
    return PACKET_NEED_MORE_DATA;
  user->buffer >> strings4;

  //ToDo: Save signs!
  signData *newSign = new signData;
  newSign->x = x;
  newSign->y = y;
  newSign->z = z;
  newSign->text1 = strings1;
  newSign->text2 = strings2;
  newSign->text3 = strings3;
  newSign->text4 = strings4;

  sChunk* chunk = Mineserver::get()->map(user->pos.map)->chunks.getChunk(blockToChunk(x),blockToChunk(z));

  if(chunk != NULL)
  {
    //Check if this sign data already exists and remove
    for(uint32_t i = 0; i < chunk->signs.size(); i++)
    {
      if(chunk->signs[i]->x == x && 
         chunk->signs[i]->y == y && 
         chunk->signs[i]->z == z)
      {
        //Erase existing data
        delete chunk->signs[i];
        chunk->signs.erase(chunk->signs.begin()+i);
        break;
      }
    }
    chunk->signs.push_back(newSign);

    //Send sign packet to everyone
    Packet pkt;
    pkt << (int8_t)PACKET_SIGN << x << y << z;
    pkt << strings1 << strings2 << strings3 << strings4;
    user->sendAll((uint8_t *)pkt.getWrite(), pkt.getWriteLen());
  }

  Mineserver::get()->logger()->log(LogType::LOG_INFO, "Packets", "Sign: " + strings1 + strings2 + strings3 + strings4);

  //No need to do anything
  user->buffer.removePacket();
  return PACKET_OK;
}


int PacketHandler::inventory_close(User *user)
{
  int8_t windowID;

  user->buffer >> windowID;

  Mineserver::get()->inventory()->windowClose(user,windowID);

  user->buffer.removePacket();
  return PACKET_OK;
}


int PacketHandler::inventory_transaction(User *user)
{
  int8_t windowID;
  int16_t action;
  int8_t accepted;

  user->buffer >> windowID >> action >> accepted;


  //No need to do anything
  user->buffer.removePacket();
  return PACKET_OK;
}

//
int PacketHandler::inventory_change(User *user)
{
  if(!user->buffer.haveData(10))
  {
    return PACKET_NEED_MORE_DATA;
  }
  int8_t windowID = 0;
  int16_t slot = 0;
  int8_t rightClick = 0;
  int16_t actionNumber = 0;
  int16_t itemID = 0;
  int8_t itemCount = 0;
  int16_t itemUses  = 0;

  user->buffer >> windowID >> slot >> rightClick >> actionNumber >> itemID;
  if(itemID != -1)
  {
    if(!user->buffer.haveData(2))
    {
      return PACKET_NEED_MORE_DATA;
    }
    user->buffer >> itemCount >> itemUses;
  }

  Mineserver::get()->inventory()->windowClick(user,windowID,slot,rightClick,actionNumber,itemID,itemCount,itemUses);
  
  //No need to do anything
  user->buffer.removePacket();
  return PACKET_OK;
}

// Keep Alive (http://mc.kev009.com/wiki/Protocol#Keep_Alive_.280x00.29)
int PacketHandler::keep_alive(User *user)
{
  //No need to do anything
  user->buffer.removePacket();
  return PACKET_OK;
}

//Source: http://wiki.linuxquestions.org/wiki/Connecting_a_socket_in_C
int socket_connect(char *host, int port)
{
  struct hostent *hp;
  struct sockaddr_in addr;
  int on = 1, sock;     

  if((hp = gethostbyname(host)) == NULL)
  {
      return 0;
  }

  memmove(&addr.sin_addr, hp->h_addr, hp->h_length);
  addr.sin_port = htons(port);
  addr.sin_family = AF_INET;
  sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

  struct timeval tv;
  tv.tv_sec = 2;
  setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,(const char *)&tv,sizeof(struct timeval));
  setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO,(const char *)&tv,sizeof(struct timeval));

  setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char *)&on, sizeof(int));

  if (sock == -1)
  {
    return 0;
  }

  if (connect(sock, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) == -1)
  {
    return 0;
  }

  return sock;
}

// Login request (http://mc.kev009.com/wiki/Protocol#Login_Request_.280x01.29)
int PacketHandler::login_request(User *user)
{
  //Check that we have enought data in the buffer
  if(!user->buffer.haveData(12))
  {
    return PACKET_NEED_MORE_DATA;
  }

  int32_t version;
  std::string player, passwd;
  int64_t mapseed;
  int8_t dimension;

  user->buffer >> version >> player >> passwd >> mapseed >> dimension;

  if(!user->buffer)
  {
    return PACKET_NEED_MORE_DATA;
  }

  user->buffer.removePacket();

  LOG(INFO, "Packets", "Player " + dtos(user->UID) + " login v." + dtos(version) + " : " + player + ":" + passwd);

  user->nick = player;

  // If version is not 2 or 3
  if(version != PROTOCOL_VERSION)
  {
    user->kick(Mineserver::get()->config()->sData("strings.wrong_protocol"));
    return PACKET_OK;
  }

  // If userlimit is reached
  
  if((int)Mineserver::get()->countUsers() >= Mineserver::get()->config()->iData("system.user_limit"))
  {
    user->kick(Mineserver::get()->config()->sData("strings.server_full"));
    return PACKET_OK;
  }

  char* kickMessage = NULL;
  if ((static_cast<Hook2<bool,const char*,char**>*>(Mineserver::get()->plugin()->getHook("PlayerLoginPre")))->doUntilFalse(player.c_str(), &kickMessage))
  {
    user->kick(std::string(kickMessage));
  }
  else
  {
    user->sendLoginInfo();
    (static_cast<Hook1<bool,const char*>*>(Mineserver::get()->plugin()->getHook("PlayerLoginPost")))->doAll(player.c_str());
  }

  return PACKET_OK;
}

int PacketHandler::handshake(User *user)
{
  if(!user->buffer.haveData(3))
    return PACKET_NEED_MORE_DATA;

  std::string player;

  user->buffer >> player;

  // Check for data
  if(!user->buffer)
    return PACKET_NEED_MORE_DATA;

  // Remove package from buffer
  user->buffer.removePacket();

  // Check whether we're to validate against minecraft.net
  if(Mineserver::get()->config()->bData("system.user_validation") == true)
  {
    // Send the unique hash for this player to prompt the client to go to minecraft.net to validate
    LOG(INFO, "Packets", "Handshake: Giving player "+player+" their minecraft.net hash of: " + hash(player));
    user->buffer << (int8_t)PACKET_HANDSHAKE << hash(player);
  }
  else
  {
    // Send "no validation or password needed" validation
    LOG(INFO, "Packets", "Handshake: No validation required for player "+player+".");
    user->buffer << (int8_t)PACKET_HANDSHAKE << std::string("-");
  }
  // TODO: Add support for prompting user for Server password (once client supports it)

  return PACKET_OK;
}

int PacketHandler::chat_message(User *user)
{
  // Wait for length-short. HEHE
  if(!user->buffer.haveData(2))
  {
    return PACKET_NEED_MORE_DATA;
  }

  std::string msg;

  user->buffer >> msg;

  if(!user->buffer)
  {
    return PACKET_NEED_MORE_DATA;
  }

  user->buffer.removePacket();

  Mineserver::get()->chat()->handleMsg(user, msg);

  return PACKET_OK;
}

int PacketHandler::player(User *user)
{
  //OnGround packet
  int8_t onground;
  user->buffer >> onground;
  if(!user->buffer)
  {
    return PACKET_NEED_MORE_DATA;
  }
  user->buffer.removePacket();
  return PACKET_OK;
}

int PacketHandler::player_position(User *user)
{
  double x, y, stance, z;
  int8_t onground;

  user->buffer >> x >> y >> stance >> z >> onground;

  if(!user->buffer)
  {
    return PACKET_NEED_MORE_DATA;
  }

  user->updatePos(x, y, z, stance);
  user->buffer.removePacket();

  return PACKET_OK;
}

int PacketHandler::player_look(User *user)
{
  float yaw, pitch;
  int8_t onground;

  user->buffer >> yaw >> pitch >> onground;

  if(!user->buffer)
  {
    return PACKET_NEED_MORE_DATA;
  }

  user->updateLook(yaw, pitch);

  user->buffer.removePacket();

  return PACKET_OK;
}

int PacketHandler::player_position_and_look(User *user)
{
  double x, y, stance, z;
  float yaw, pitch;
  int8_t onground;

  user->buffer >> x >> y >> stance >> z
               >> yaw >> pitch >> onground;

  if(!user->buffer)
  {
    return PACKET_NEED_MORE_DATA;
  }

  //Update user data
  user->updatePos(x, y, z, stance);
  user->updateLook(yaw, pitch);

  user->buffer.removePacket();

  return PACKET_OK;
}

int PacketHandler::player_digging(User *user)
{
  int8_t status;
  int32_t x;
  int8_t  y;
  int32_t z;
  int8_t direction;
  uint8_t block;
  uint8_t meta;
  BlockBasic* blockcb;


  user->buffer >> status >> x >> y >> z >> direction;

  if(!user->buffer)
  {
    return PACKET_NEED_MORE_DATA;
  }

  user->buffer.removePacket();

  if(!Mineserver::get()->map(user->pos.map)->getBlock(x, y, z, &block, &meta))
  {
    return PACKET_OK;
  }

  switch(status)
  {
    case BLOCK_STATUS_STARTED_DIGGING:
    {
      (static_cast<Hook5<bool,const char*,int32_t,int8_t,int32_t,int8_t>*>(Mineserver::get()->plugin()->getHook("PlayerDiggingStarted")))->doAll(user->nick.c_str(), x, y, z, direction);

      for(uint32_t i =0 ; i<Mineserver::get()->plugin()->getBlockCB().size(); i++)
      {
        blockcb = Mineserver::get()->plugin()->getBlockCB()[i];
        if(blockcb!=NULL && blockcb->affectedBlock(block))
        {
          blockcb->onStartedDigging(user, status,x,y,z,user->pos.map,direction);
        }
      }
      break;
    }
    case BLOCK_STATUS_DIGGING:
    {
      (static_cast<Hook5<bool,const char*,int32_t,int8_t,int32_t,int8_t>*>(Mineserver::get()->plugin()->getHook("PlayerDigging")))->doAll(user->nick.c_str(), x, y, z, direction);
      (static_cast<Hook4<bool,const char*,int32_t,int8_t,int32_t>*>(Mineserver::get()->plugin()->getHook("PlayerDigging")))->doAll(user->nick.c_str(), x, y, z);
      for(uint32_t i =0 ; i<Mineserver::get()->plugin()->getBlockCB().size(); i++)
      {
        blockcb = Mineserver::get()->plugin()->getBlockCB()[i];
        if(blockcb!=NULL && blockcb->affectedBlock(block))
        {
          blockcb->onDigging(user, status,x,y,z,user->pos.map,direction);
        }
      }

      break;
    }
    case BLOCK_STATUS_STOPPED_DIGGING:
    {
      (static_cast<Hook5<bool,const char*,int32_t,int8_t,int32_t,int8_t>*>(Mineserver::get()->plugin()->getHook("PlayerDiggingStopped")))->doAll(user->nick.c_str(), x, y, z, direction);
      for(uint32_t i =0 ; i<Mineserver::get()->plugin()->getBlockCB().size(); i++)
      {
        blockcb = Mineserver::get()->plugin()->getBlockCB()[i];
        if(blockcb!=NULL && blockcb->affectedBlock(block))
        {
          blockcb->onStoppedDigging(user,status, x,y,z,user->pos.map,direction);
        }
      }
      break;
    }
    case BLOCK_STATUS_BLOCK_BROKEN:
    {
      //Player tool usage calculation etc
      #define itemSlot (36+user->currentItemSlot())
      bool rightUse;
      int16_t itemHealth=Mineserver::get()->inventory()->itemHealth(user->inv[itemSlot].type,block,rightUse);
      if(itemHealth > 0)
      {
         user->inv[itemSlot].health++;
         if(!rightUse)
           user->inv[itemSlot].health++;
         if(itemHealth <= user->inv[itemSlot].health)
         {
            user->inv[itemSlot].count--;
            if(user->inv[itemSlot].count == 0)
            {
              user->inv[itemSlot] = Item();
            }            
         }
         Mineserver::get()->inventory()->setSlot(user,WINDOW_PLAYER,itemSlot,user->inv[itemSlot].type,
                                                 user->inv[itemSlot].count,user->inv[itemSlot].health);
      }
      #undef itemSlot

      if ((static_cast<Hook4<bool,const char*,int32_t,int8_t,int32_t>*>(Mineserver::get()->plugin()->getHook("BlockBreakPre")))->doUntilFalse(user->nick.c_str(), x, y, z))
      {
        return PACKET_OK;
      }

      (static_cast<Hook4<bool,const char*,int32_t,int8_t,int32_t>*>(Mineserver::get()->plugin()->getHook("BlockBreakPost")))->doAll(user->nick.c_str(), x, y, z);

      for(uint32_t i =0 ; i<Mineserver::get()->plugin()->getBlockCB().size(); i++)
      {
        blockcb = Mineserver::get()->plugin()->getBlockCB()[i];
        if(blockcb!=NULL && blockcb->affectedBlock(block))
        {
          if(blockcb->onBroken(user, status,x,y,z,user->pos.map,direction))
          {
            // Do not break
            return PACKET_OK;
          }
          else
          {
            break;
          }
        }
      }

      /* notify neighbour blocks of the broken block */
      status = block;
      if (Mineserver::get()->map(user->pos.map)->getBlock(x+1, y, z, &block, &meta) && block != BLOCK_AIR)
      {
        (static_cast<Hook7<bool,const char*,int32_t,int8_t,int32_t,int32_t,int8_t,int32_t>*>(Mineserver::get()->plugin()->getHook("BlockNeighbourBreak")))->doAll(user->nick.c_str(), x+1, y, z, x, y, z);
        for(uint32_t i =0 ; i<Mineserver::get()->plugin()->getBlockCB().size(); i++)
        {
          blockcb = Mineserver::get()->plugin()->getBlockCB()[i];
          if(blockcb!=NULL && (blockcb->affectedBlock(status)||blockcb->affectedBlock(block)))
          {
            blockcb->onNeighbourBroken(user, status,x+1,y,z,user->pos.map,BLOCK_SOUTH);
          }
        }

      }

      if (Mineserver::get()->map(user->pos.map)->getBlock(x-1, y, z, &block, &meta) && block != BLOCK_AIR)
      {
        (static_cast<Hook7<bool,const char*,int32_t,int8_t,int32_t,int32_t,int8_t,int32_t>*>(Mineserver::get()->plugin()->getHook("BlockNeighbourBreak")))->doAll(user->nick.c_str(), x-1, y, z, x, y, z);
        for(uint32_t i =0 ; i<Mineserver::get()->plugin()->getBlockCB().size(); i++)
        {
          blockcb = Mineserver::get()->plugin()->getBlockCB()[i];
          if(blockcb!=NULL && (blockcb->affectedBlock(status)||blockcb->affectedBlock(block)))
          {
            blockcb->onNeighbourBroken(user, status,x-1,y,z,user->pos.map,BLOCK_NORTH);
          }
        }

      }

      if (Mineserver::get()->map(user->pos.map)->getBlock(x, y+1, z, &block, &meta) && block != BLOCK_AIR)
      {
        (static_cast<Hook7<bool,const char*,int32_t,int8_t,int32_t,int32_t,int8_t,int32_t>*>(Mineserver::get()->plugin()->getHook("BlockNeighbourBreak")))->doAll(user->nick.c_str(), x, y+1, z, x, y, z);
        for(uint32_t i =0 ; i<Mineserver::get()->plugin()->getBlockCB().size(); i++)
        {
          blockcb = Mineserver::get()->plugin()->getBlockCB()[i];
          if(blockcb!=NULL && (blockcb->affectedBlock(status)||blockcb->affectedBlock(block)))
          {
            blockcb->onNeighbourBroken(user, status,x,y+1,z,user->pos.map,BLOCK_TOP);
          }
        }

      }

      if (Mineserver::get()->map(user->pos.map)->getBlock(x, y-1, z, &block, &meta) && block != BLOCK_AIR)
      {
        (static_cast<Hook7<bool,const char*,int32_t,int8_t,int32_t,int32_t,int8_t,int32_t>*>(Mineserver::get()->plugin()->getHook("BlockNeighbourBreak")))->doAll(user->nick.c_str(), x, y-1, z, x, y, z);
        for(uint32_t i =0 ; i<Mineserver::get()->plugin()->getBlockCB().size(); i++)
        {
          blockcb = Mineserver::get()->plugin()->getBlockCB()[i];
          if(blockcb!=NULL && (blockcb->affectedBlock(status)||blockcb->affectedBlock(block)))
          {
            blockcb->onNeighbourBroken(user, status,x,y-1,z,user->pos.map,BLOCK_BOTTOM);
          }
        }

      }

      if (Mineserver::get()->map(user->pos.map)->getBlock(x, y, z+1, &block, &meta) && block != BLOCK_AIR)
      {
        (static_cast<Hook7<bool,const char*,int32_t,int8_t,int32_t,int32_t,int8_t,int32_t>*>(Mineserver::get()->plugin()->getHook("BlockNeighbourBreak")))->doAll(user->nick.c_str(), x, y, z+1, x, y, z);
        for(uint32_t i =0 ; i<Mineserver::get()->plugin()->getBlockCB().size(); i++)
        {
          blockcb = Mineserver::get()->plugin()->getBlockCB()[i];
          if(blockcb!=NULL && (blockcb->affectedBlock(status)||blockcb->affectedBlock(block)))
          {
            blockcb->onNeighbourBroken(user, status,x,y,z+1,user->pos.map,BLOCK_WEST);
          }
        }

      }

      if (Mineserver::get()->map(user->pos.map)->getBlock(x, y, z-1, &block, &meta) && block != BLOCK_AIR)
      {
        (static_cast<Hook7<bool,const char*,int32_t,int8_t,int32_t,int32_t,int8_t,int32_t>*>(Mineserver::get()->plugin()->getHook("BlockNeighbourBreak")))->doAll(user->nick.c_str(), x, y, z-1, x, y, z);
        for(uint32_t i =0 ; i<Mineserver::get()->plugin()->getBlockCB().size(); i++)
        {
          blockcb = Mineserver::get()->plugin()->getBlockCB()[i];
          if(blockcb!=NULL && (blockcb->affectedBlock(status)||blockcb->affectedBlock(block)))
          {
            blockcb->onNeighbourBroken(user, status,x,y,z-1,user->pos.map,BLOCK_EAST);
          }
        }

      }

      break;
    }
    case BLOCK_STATUS_PICKUP_SPAWN:
    {
      //ToDo: handle
      #define itemSlot (36+user->currentItemSlot())
      if(user->inv[itemSlot].type > 0)
      {
        Mineserver::get()->map(user->pos.map)->createPickupSpawn(user->pos.x, user->pos.y,user->pos.z,user->inv[itemSlot].type,1,user->inv[itemSlot].health,user);

        user->inv[itemSlot].count--;
        if(user->inv[itemSlot].count == 0)
        {
          user->inv[itemSlot] = Item();
        }
        Mineserver::get()->inventory()->setSlot(user,WINDOW_PLAYER,itemSlot,user->inv[itemSlot].type,
                                                user->inv[itemSlot].count,user->inv[itemSlot].health);
      }
      break;
      #undef itemSlot
    }
    
  }

  return PACKET_OK;
}

int PacketHandler::player_block_placement(User *user)
{
  if(!user->buffer.haveData(12))
  {
    return PACKET_NEED_MORE_DATA;
  }
  int8_t y = 0, direction = 0;
  int16_t newblock = 0;
  int32_t x, z = 0;
  /* replacement of block */
  uint8_t oldblock = 0;
  uint8_t metadata = 0;
  /* neighbour blocks */
  uint8_t block = 0;
  uint8_t meta  = 0;
  int8_t count  = 0;
  int16_t health = 0;
  BlockBasic* blockcb;


  user->buffer >> x >> y >> z >> direction >> newblock;

  if(newblock >= 0)
  {
    if(!user->buffer.haveData(2))
    {
      return PACKET_NEED_MORE_DATA;
    }
    user->buffer >> count >> health;
  }
  user->buffer.removePacket();


  if (!Mineserver::get()->map(user->pos.map)->getBlock(x, y, z, &oldblock, &metadata))
  {
    return PACKET_OK;
  }
  
  /* Protocol docs say this should be what interacting is. */
  if(oldblock != BLOCK_AIR)
  {
    (static_cast<Hook4<bool,const char*,int32_t,int8_t,int32_t>*>(Mineserver::get()->plugin()->getHook("PlayerBlockInteract")))->doAll(user->nick.c_str(), x, y, z);
      for(uint32_t i =0 ; i<Mineserver::get()->plugin()->getBlockCB().size(); i++)
      {
        blockcb = Mineserver::get()->plugin()->getBlockCB()[i];
        if(blockcb!=NULL && blockcb->affectedBlock(oldblock))
        {
          //This should actually make the boolean do something. Maybe.
          if(blockcb->onInteract(user, x,y,z,user->pos.map))
          {
            return PACKET_OK;
          }
          else
          {
            break;
          }
        }
      }
  }
  bool foundFromInventory = false;

  #define INV_TASKBAR_START 36
  if(user->inv[INV_TASKBAR_START+user->currentItemSlot()].type == newblock && newblock != -1)
  {
    foundFromInventory = true;
  }
  #undef INV_TASKBAR_START


  // TODO: Handle processing of
  if(direction == -1 || !foundFromInventory)
  {
    return PACKET_OK;
  }

  // Minecart testing!!
  if(newblock == ITEM_MINECART && Mineserver::get()->map(user->pos.map)->getBlock(x, y, z, &oldblock, &metadata))
  {
    if (oldblock != BLOCK_MINECART_TRACKS)
    {
      return PACKET_OK;
    }

    LOG(INFO, "Packets", "Spawn minecart");
    int32_t EID=generateEID();
    Packet pkt;
    // MINECART
    pkt << PACKET_ADD_OBJECT << (int32_t)EID << (int8_t)10 << (int32_t)(x*32+16) << (int32_t)(y*32) << (int32_t)(z*32+16);
    user->sendAll((uint8_t *)pkt.getWrite(), pkt.getWriteLen());
  }

  if (newblock == -1 && newblock != ITEM_SIGN)
  {
#ifdef _DEBUG
     LOG(DEBUG, "Packets", "ignoring: "+dtos(newblock));
#endif
     return PACKET_OK;
  }

  if(y < 0)
  {
    return PACKET_OK;
  }

#ifdef _DEBUG
  LOG(DEBUG,"Packets", "Block_placement: "+dtos(newblock)+" ("+dtos(x)+","+dtos((int)y)+","+dtos(z)+") dir: "+dtos((int)direction));
#endif

  if (direction)
  {
    direction = 6-direction;
  }

  //if (Mineserver::get()->map()->getBlock(x, y, z, &oldblock, &metadata))
  {
    uint8_t oldblocktop;
    uint8_t metadatatop;
    int8_t check_y = y;
    int32_t check_x = x;
    int32_t check_z = z;

    /* client doesn't give us the correct block for lava and water, check block above */
    switch(direction)
    {
      case BLOCK_BOTTOM: check_y--;  break;
      case BLOCK_TOP:    check_y++;  break;
      case BLOCK_NORTH:  check_x++;  break;
      case BLOCK_SOUTH:  check_x--;  break;
      case BLOCK_EAST:   check_z++;  break;
      case BLOCK_WEST:   check_z--;  break;
      default:                       break;
    }

    if (Mineserver::get()->map(user->pos.map)->getBlock(check_x, check_y, check_z, &oldblocktop, &metadatatop) && 
        (oldblocktop == BLOCK_LAVA || oldblocktop == BLOCK_STATIONARY_LAVA ||
         oldblocktop == BLOCK_WATER || oldblocktop == BLOCK_STATIONARY_WATER))
    {
      /* block above needs replacing rather then the block sent by the client */

      // TODO: Does this require some form of recursion for multiple water/lava blocks?

      for(uint32_t i =0 ; i<Mineserver::get()->plugin()->getBlockCB().size(); i++)
      {
        blockcb = Mineserver::get()->plugin()->getBlockCB()[i];
        if(blockcb!=NULL && blockcb->affectedBlock(newblock))
        {
          blockcb->onReplace(user, newblock,check_x,check_y,check_z,user->pos.map,direction);
        }
      }

      if ((static_cast<Hook6<bool,const char*,int32_t,int8_t,int32_t,int16_t,int16_t>*>(Mineserver::get()->plugin()->getHook("BlockReplacePre")))->doUntilFalse(user->nick.c_str(), check_x, check_y, check_z, oldblock, newblock))
      {
        return PACKET_OK;
      }
      (static_cast<Hook6<bool,const char*,int32_t,int8_t,int32_t,int16_t,int16_t>*>(Mineserver::get()->plugin()->getHook("BlockReplacePost")))->doAll(user->nick.c_str(), check_x, check_y, check_z, oldblock, newblock);
    }
    else
    {
/*      for(int i =0 ; i<Mineserver::get()->plugin()->getBlockCB().size(); i++)
      {
        blockcb = Mineserver::get()->plugin()->getBlockCB()[i];
        if(blockcb!=NULL && blockcb->affectedBlock(newblock))
        {
          blockcb->onReplace(user, newblock,check_x,check_y,check_z,user->pos.map,direction);
        }
      }*/

      if ((static_cast<Hook6<bool,const char*,int32_t,int8_t,int32_t,int16_t,int16_t>*>(Mineserver::get()->plugin()->getHook("BlockReplacePre")))->doUntilFalse(user->nick.c_str(), x, y, z, oldblock, newblock))
      {
        return PACKET_OK;
      }
      (static_cast<Hook6<bool,const char*,int32_t,int8_t,int32_t,int16_t,int16_t>*>(Mineserver::get()->plugin()->getHook("BlockReplacePost")))->doAll(user->nick.c_str(), x, y, z, oldblock, newblock);
    }

    if ((static_cast<Hook6<bool,const char*,int32_t,int8_t,int32_t,int16_t,int8_t>*>(Mineserver::get()->plugin()->getHook("BlockPlacePre")))->doUntilFalse(user->nick.c_str(), x, y, z, newblock,direction))
    {
      return PACKET_OK;
    }

    /* We pass the newblock to the newblock's onPlace callback because
    the callback doesn't know what type of block we're placing. Instead
    the callback's job is to describe the behaviour when placing the
    block down, not to place any specifically block itself. */
    for(uint32_t i =0 ; i<Mineserver::get()->plugin()->getBlockCB().size(); i++)
    {
      blockcb = Mineserver::get()->plugin()->getBlockCB()[i];
      if(blockcb!=NULL && blockcb->affectedBlock(newblock))
      {
        if(blockcb->onPlace(user, newblock,x,y,z,user->pos.map,direction)){
          return PACKET_OK;
        }else{
          break;
        }
      }
    }
    (static_cast<Hook6<bool,const char*,int32_t,int8_t,int32_t,int16_t,int8_t>*>(Mineserver::get()->plugin()->getHook("BlockPlacePost")))->doAll(user->nick.c_str(), x, y, z, newblock,direction);

    /* notify neighbour blocks of the placed block */
    if (Mineserver::get()->map(user->pos.map)->getBlock(x+1, y, z, &block, &meta) && block != BLOCK_AIR)
    {
      for(uint32_t i =0 ; i<Mineserver::get()->plugin()->getBlockCB().size(); i++)
      {
        blockcb = Mineserver::get()->plugin()->getBlockCB()[i];
        if(blockcb!=NULL && blockcb->affectedBlock(newblock))
        {
          blockcb->onNeighbourPlace(user, newblock,x+1,y,z,user->pos.map,direction);
        }
      }

      (static_cast<Hook4<bool,const char*,int32_t,int8_t,int32_t>*>(Mineserver::get()->plugin()->getHook("BlockNeighbourPlace")))->doAll(user->nick.c_str(), x+1, y, z);
    }

    if (Mineserver::get()->map(user->pos.map)->getBlock(x-1, y, z, &block, &meta) && block != BLOCK_AIR)
    {
      for(uint32_t i =0 ; i<Mineserver::get()->plugin()->getBlockCB().size(); i++)
      { 
        blockcb = Mineserver::get()->plugin()->getBlockCB()[i];
        if(blockcb!=NULL && blockcb->affectedBlock(newblock))
        { 
          blockcb->onNeighbourPlace(user, newblock,x-1,y,z,user->pos.map,direction);
        } 
      } 
      (static_cast<Hook4<bool,const char*,int32_t,int8_t,int32_t>*>(Mineserver::get()->plugin()->getHook("BlockNeighbourPlace")))->doAll(user->nick.c_str(), x-1, y, z);
    }

    if (Mineserver::get()->map(user->pos.map)->getBlock(x, y+1, z, &block, &meta) && block != BLOCK_AIR)
    {
      for(uint32_t i =0 ; i<Mineserver::get()->plugin()->getBlockCB().size(); i++)
      { 
        blockcb = Mineserver::get()->plugin()->getBlockCB()[i];
        if(blockcb!=NULL && blockcb->affectedBlock(newblock))
        { 
          blockcb->onNeighbourPlace(user, newblock,x,y+1,z,user->pos.map,direction);
        } 
      } 
      (static_cast<Hook4<bool,const char*,int32_t,int8_t,int32_t>*>(Mineserver::get()->plugin()->getHook("BlockNeighbourPlace")))->doAll(user->nick.c_str(), x, y+1, z);
    }

    if (Mineserver::get()->map(user->pos.map)->getBlock(x, y-1, z, &block, &meta) && block != BLOCK_AIR)
    {
      for(uint32_t i =0 ; i<Mineserver::get()->plugin()->getBlockCB().size(); i++)
      { 
        blockcb = Mineserver::get()->plugin()->getBlockCB()[i];
        if(blockcb!=NULL && blockcb->affectedBlock(newblock))
        { 
          blockcb->onNeighbourPlace(user, newblock,x,y-1,z,user->pos.map,direction);
        } 
      } 
      (static_cast<Hook4<bool,const char*,int32_t,int8_t,int32_t>*>(Mineserver::get()->plugin()->getHook("BlockNeighbourPlace")))->doAll(user->nick.c_str(), x, y-1, z);
    }

    if (Mineserver::get()->map(user->pos.map)->getBlock(x, y, z+1, &block, &meta) && block != BLOCK_AIR)
    {
      for(uint32_t i =0 ; i<Mineserver::get()->plugin()->getBlockCB().size(); i++)
      { 
        blockcb = Mineserver::get()->plugin()->getBlockCB()[i];
        if(blockcb!=NULL && blockcb->affectedBlock(newblock))
        { 
          blockcb->onNeighbourPlace(user, newblock,x,y,z+1,user->pos.map,direction);
        } 
      } 
      (static_cast<Hook4<bool,const char*,int32_t,int8_t,int32_t>*>(Mineserver::get()->plugin()->getHook("BlockNeighbourPlace")))->doAll(user->nick.c_str(), x, y, z+1);
    }

    if (Mineserver::get()->map(user->pos.map)->getBlock(x, y, z-1, &block, &meta) && block != BLOCK_AIR)
    {
      for(uint32_t i =0 ; i<Mineserver::get()->plugin()->getBlockCB().size(); i++)
      { 
        blockcb = Mineserver::get()->plugin()->getBlockCB()[i];
        if(blockcb!=NULL && blockcb->affectedBlock(newblock))
        { 
          blockcb->onNeighbourPlace(user, newblock,x,y,z-1,user->pos.map,direction);
        } 
      } 
      (static_cast<Hook4<bool,const char*,int32_t,int8_t,int32_t>*>(Mineserver::get()->plugin()->getHook("BlockNeighbourPlace")))->doAll(user->nick.c_str(), x, y, z-1);
    }
  }
  // Now we're sure we're using it, lets remove from inventory!
  #define INV_TASKBAR_START 36
  if(user->inv[INV_TASKBAR_START+user->currentItemSlot()].type == newblock && newblock != -1)
  {
    //if(newblock<256)
    {
      // It's a block
      user->inv[INV_TASKBAR_START+user->currentItemSlot()].count--;
      if(user->inv[INV_TASKBAR_START+user->currentItemSlot()].count == 0)
      {
        user->inv[INV_TASKBAR_START+user->currentItemSlot()] = Item();
        //ToDo: add holding change packet.
      }
      user->buffer << (int8_t)PACKET_SET_SLOT << (int8_t)WINDOW_PLAYER
                   << (int16_t)(INV_TASKBAR_START+user->currentItemSlot())
                   << (int16_t)user->inv[INV_TASKBAR_START+user->currentItemSlot()].type;
      if(user->inv[INV_TASKBAR_START+user->currentItemSlot()].type != -1)
      {
        user->buffer << (int8_t)user->inv[INV_TASKBAR_START+user->currentItemSlot()].count
                     << (int16_t)user->inv[INV_TASKBAR_START+user->currentItemSlot()].health;
      }
    }
  }
  #undef INV_TASKBAR_START



  /* TODO: Should be removed from here. Only needed for liquid related blocks? */
  Mineserver::get()->physics(user->pos.map)->checkSurrounding(vec(x, y, z));
  return PACKET_OK;
}

int PacketHandler::holding_change(User *user)
{
  int16_t itemSlot;
  user->buffer >> itemSlot;

  if(!user->buffer)
  {
    return PACKET_NEED_MORE_DATA;
  }

  user->buffer.removePacket();

  user->curItem = itemSlot;

  //Send holding change to others
  Packet pkt;
  pkt << (int8_t)PACKET_ENTITY_EQUIPMENT << (int32_t)user->UID << (int16_t)0 << (int16_t)user->inv[itemSlot+36].type << (int16_t)user->inv[itemSlot+36].health;
  user->sendOthers((uint8_t*)pkt.getWrite(), pkt.getWriteLen());

  // Set current itemID to user
  user->setCurrentItemSlot(itemSlot);

  return PACKET_OK;
}

int PacketHandler::arm_animation(User *user)
{
  int32_t userID;
  int8_t animType;

  user->buffer >> userID >> animType;

  if(!user->buffer)
  {
    return PACKET_NEED_MORE_DATA;
  }

  user->buffer.removePacket();

  Packet pkt;
  pkt << (int8_t)PACKET_ARM_ANIMATION << (int32_t)user->UID << animType;
  user->sendOthers((uint8_t*)pkt.getWrite(), pkt.getWriteLen());

  (static_cast<Hook1<bool,const char*>*>(Mineserver::get()->plugin()->getHook("PlayerArmSwing")))->doAll(user->nick.c_str());

  return PACKET_OK;
}

int PacketHandler::pickup_spawn(User *user)
{
  //uint32_t curpos = 4; //warning: unused variable ‘curpos’
  spawnedItem item;

  item.health = 0;

  int8_t yaw, pitch, roll;

  user->buffer >> (int32_t&)item.EID;

  user->buffer >> (int16_t&)item.item >> (int8_t&)item.count >> (int16_t&)item.health;
  user->buffer >> (int32_t&)item.pos.x() >> (int32_t&)item.pos.y() >> (int32_t&)item.pos.z();
  user->buffer >> yaw >> pitch >> roll;

  if(!user->buffer)
  {
    return PACKET_NEED_MORE_DATA;
  }

  user->buffer.removePacket();

  item.EID       = generateEID();

  item.spawnedBy = user->UID;
  item.spawnedAt = time(NULL);

  // Modify the position of the dropped item so that it appears in front of user instead of under user
  int distanceToThrow = 64;
  float angle = DEGREES_TO_RADIANS(user->pos.yaw + 45);     // For some reason, yaw seems to be off to the left by 45 degrees from where you're actually looking?
  int x = int(cos(angle) * distanceToThrow - sin(angle) * distanceToThrow);
  int z = int(sin(angle) * distanceToThrow + cos(angle) * distanceToThrow);
  item.pos += vec(x, 0, z);

  Mineserver::get()->map(user->pos.map)->sendPickupSpawn(item);

  return PACKET_OK;
}

int PacketHandler::disconnect(User *user)
{
  if(!user->buffer.haveData(2))
    return PACKET_NEED_MORE_DATA;

  std::string msg;
  user->buffer >> msg;

  if(!user->buffer)
  {
    return PACKET_NEED_MORE_DATA;
  }

  user->buffer.removePacket();

  LOG(INFO, "Packets", "Disconnect: " + msg);

  return PACKET_OK;
}


int PacketHandler::use_entity(User *user)
{
  int32_t userID, target;
  int8_t targetType;

  user->buffer >> userID >> target >> targetType;

  if (!user->buffer)
  {
    return PACKET_NEED_MORE_DATA;
  }

  user->buffer.removePacket();

  if(targetType != 1)
  {

    Packet pkt;
    //Attach
    if(user->attachedTo == 0)
    {
      pkt << PACKET_ATTACH_ENTITY << (int32_t)user->UID << (int32_t)target;
      user->attachedTo = target;
    }
    //Detach
    else
    {
      pkt << PACKET_ATTACH_ENTITY << (int32_t)user->UID << (int32_t)-1;
      user->attachedTo = 0;
    }
    user->sendAll((uint8_t*)pkt.getWrite(), pkt.getWriteLen());
    return PACKET_OK;
  }

  if(Mineserver::get()->m_pvp_enabled)
  {
    //This is used when punching users, mobs or other entities
    for (std::vector<User*>::iterator it = Mineserver::get()->usersBegin();
        it != Mineserver::get()->usersEnd();
        ++it)
    {
      User* user = *it;
      if(user->UID == (uint32_t)target)
      {
        user->health--;
        user->sethealth(user->health);

        if(user->health <= 0)
        {
          Packet pkt;
          pkt << PACKET_DEATH_ANIMATION << (int32_t)user->UID << (int8_t)3;
          user->sendOthers((uint8_t*)pkt.getWrite(), pkt.getWriteLen());
        }
        break;
      }
    }
  }

  return PACKET_OK;
}

int PacketHandler::respawn(User *user)
{
  user->dropInventory();
  user->respawn();
  user->buffer.removePacket();
  return PACKET_OK;
}

// Shift operators for Packet class
Packet & Packet::operator<<(int8_t val)
{
  m_writeBuffer.push_back(val);
  return *this;
}

Packet & Packet::operator>>(int8_t &val)
{
  if(haveData(1))
  {
    val = *reinterpret_cast<const int8_t*>(&m_readBuffer[m_readPos]);
    m_readPos += 1;
  }
  return *this;
}

Packet & Packet::operator<<(int16_t val)
{
  uint16_t nval = htons(val);
  addToWrite(&nval, 2);
  return *this;
}

Packet & Packet::operator>>(int16_t &val)
{
  if(haveData(2))
  {
    val = ntohs(*reinterpret_cast<const int16_t*>(&m_readBuffer[m_readPos]));
    m_readPos += 2;
  }
  return *this;
}

Packet & Packet::operator<<(int32_t val)
{
  uint32_t nval = htonl(val);
  addToWrite(&nval, 4);
  return *this;
}

Packet & Packet::operator>>(int32_t &val)
{
  if(haveData(4))
  {
    val = ntohl(*reinterpret_cast<const int32_t*>(&m_readBuffer[m_readPos]));
    m_readPos += 4;
  }
  return *this;
}

Packet & Packet::operator<<(int64_t val)
{
  uint64_t nval = ntohll(val);
  addToWrite(&nval, 8);
  return *this;
}

Packet & Packet::operator>>(int64_t &val)
{
  if(haveData(8))
  {
    val = *reinterpret_cast<const int64_t*>(&m_readBuffer[m_readPos]);
    val = ntohll(val);
    m_readPos += 8;
  }
  return *this;
}

Packet & Packet::operator<<(float val)
{
  uint32_t nval;
  memcpy(&nval, &val , 4);
  nval = htonl(nval);
  addToWrite(&nval, 4);
  return *this;
}

Packet & Packet::operator>>(float &val)
{
  if(haveData(4))
  {
    int32_t ival = ntohl(*reinterpret_cast<const int32_t*>(&m_readBuffer[m_readPos]));
    memcpy(&val, &ival, 4);
    m_readPos += 4;
  }
  return *this;
}

Packet & Packet::operator<<(double val)
{
  uint64_t nval;
  memcpy(&nval, &val, 8);
  nval = ntohll(nval);
  addToWrite(&nval, 8);
  return *this;
}


Packet & Packet::operator>>(double &val)
{
  if(haveData(8))
  {
    uint64_t ival = *reinterpret_cast<const uint64_t*>(&m_readBuffer[m_readPos]);
    ival = ntohll(ival);
    memcpy((void*)&val, (void*)&ival, 8);
    m_readPos += 8;
  }
  return *this;
}

Packet & Packet::operator<<(const std::string &str)
{
  uint16_t lenval = htons(str.size());
  addToWrite(&lenval, 2);

  addToWrite(&str[0], str.size());
  return *this;
}

Packet & Packet::operator>>(std::string &str)
{
  uint16_t lenval;
  if(haveData(2))
  {
    lenval = ntohs(*reinterpret_cast<const int16_t*>(&m_readBuffer[m_readPos]));
    m_readPos += 2;

    if(haveData(lenval))
    {
      str.assign((char*)&m_readBuffer[m_readPos], lenval);
      m_readPos += lenval;
    }
  }    
  return *this;
}

void Packet::operator<<(Packet &other)
{
  int dataSize = other.getWriteLen();
  if(dataSize == 0)
    return;
  BufferVector::size_type start = m_writeBuffer.size();
  m_writeBuffer.resize(start + dataSize);
  memcpy(&m_writeBuffer[start], other.getWrite(), dataSize);
}
