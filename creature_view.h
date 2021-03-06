/* Copyright (C) 2013-2014 Michal Brzozowski (rusolis@poczta.fm)

   This file is part of KeeperRL.

   KeeperRL is free software; you can redistribute it and/or modify it under the terms of the
   GNU General Public License as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   KeeperRL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
   even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along with this program.
   If not, see http://www.gnu.org/licenses/ . */

#ifndef _CREATURE_VIEW_H
#define _CREATURE_VIEW_H

#include "map_memory.h"
#include "view.h"

class Tribe;

class CreatureView {
  public:
  virtual const MapMemory& getMemory() const = 0;
  virtual ViewIndex getViewIndex(Vec2 pos) const = 0;
  virtual void refreshGameInfo(View::GameInfo&) const = 0;
  virtual Vec2 getPosition() const = 0;
  virtual bool staticPosition() const { return true; }
  virtual bool canSee(const Creature*) const = 0;
  virtual bool canSee(Vec2 position) const = 0;
  virtual const Level* getLevel() const = 0;
  virtual vector<const Creature*> getUnknownAttacker() const = 0;
  virtual Tribe* getTribe() const = 0;
  virtual bool isEnemy(const Creature*) const = 0;

  void updateVisibleCreatures();
  vector<const Creature*> getVisibleEnemies() const;
  vector<const Creature*> getVisibleFriends() const;

  template <class Archive> 
  void serialize(Archive& ar, const unsigned int version);

  virtual ~CreatureView() {}

  SERIAL_CHECKER;

  private:
  vector<const Creature*> SERIAL(visibleEnemies);
  vector<const Creature*> SERIAL(visibleFriends);
};

#endif
