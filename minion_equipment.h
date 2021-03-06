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

#ifndef _MINION_EQUIPMENT_H
#define _MINION_EQUIPMENT_H

#include "util.h"
#include "unique_entity.h"

class Creature;
class Item;

class MinionEquipment {
  public:
  bool canTakeItem(const Creature*, const Item*);

  bool isItemUseful(const Item*) const;
  bool needs(const Creature* c, const Item* it, bool noLimit = false);
  const Creature* getOwner(const Item*) const;
  void own(const Creature*, const Item*);
  void discard(const Item*);

  template <class Archive>
  void serialize(Archive& ar, const unsigned int version);

  SERIAL_CHECKER;

  private:
  enum EquipmentType { ARMOR, HEALING, ARCHERY, COMBAT_ITEM };

  static Optional<EquipmentType> getEquipmentType(const Item* it);
  int getEquipmentLimit(EquipmentType type);

  map<UniqueId, const Creature*> SERIAL(owners);
};

#endif
