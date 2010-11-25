/*
   Copyright (c) 2010, The Mineserver Project
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

#include "falling.h"

void BlockFalling::onStartedDigging(User* user, sint8 status, sint32 x, sint8 y, sint32 z, sint8 direction)
{

}

void BlockFalling::onDigging(User* user, sint8 status, sint32 x, sint8 y, sint32 z, sint8 direction)
{

}

void BlockFalling::onStoppedDigging(User* user, sint8 status, sint32 x, sint8 y, sint32 z, sint8 direction)
{

}

void BlockFalling::onBroken(User* user, sint8 status, sint32 x, sint8 y, sint32 z, sint8 direction)
{
}

void BlockFalling::onNeighbourBroken(User* user, sint8 status, sint32 x, sint8 y, sint32 z, sint8 direction)
{
   uint8 block; uint8 meta;
/*
   if (Map::get().getBlock(x, y-1, z, &block, &meta) && block == BLOCK_AIR)
   {
       if (Map::get().getBlock(x, y, z, &block, &meta) && (block == BLOCK_SNOW ||
                                                       block == BLOCK_BROWN_MUSHROOM ||
                                                       block == BLOCK_RED_MUSHROOM ||
                                                       block == BLOCK_YELLOW_FLOWER ||
                                                       block == BLOCK_RED_ROSE ||
                                                       block == BLOCK_SAPLING))
       {
         Map::get().sendBlockChange(x, y+1, z, 0, 0);
         Map::get().setBlock(x, y+1, z, 0, 0);
         //Others than snow will spawn
         if(block != BLOCK_SNOW)
         {
            Map::get().createPickupSpawn(x, y+1, z, block, 1);
         }
       }
   }*/
}

void BlockFalling::onPlace(User* user, sint8 block, sint32 x, sint8 y, sint32 z, sint8 direction)
{
   uint8 topblock;
   uint8 topmeta;
   if (Map::get().getBlock(x, y+1, z, &topblock, &topmeta))
   {
      Map::get().setBlock(x, y+1, z, (char)block, direction);
      Map::get().sendBlockChange(x, y+1, z, (char)block, direction);
   }
}

