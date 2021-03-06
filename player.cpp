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

#include "stdafx.h"

#include "player.h"
#include "location.h"
#include "level.h"
#include "message_buffer.h"
#include "ranged_weapon.h"
#include "name_generator.h"
#include "model.h"
#include "options.h"

template <class Archive> 
void Player::serialize(Archive& ar, const unsigned int version) {
  ar& SUBCLASS(Controller)
    & SUBCLASS(EventListener) 
    & SVAR(creature)
    & SVAR(travelling)
    & SVAR(travelDir)
    & SVAR(target)
    & SVAR(lastLocation)
    & SVAR(specialCreatures)
    & SVAR(displayGreeting)
    & SVAR(levelMemory)
    & SVAR(model);
  CHECK_SERIAL;
}

SERIALIZABLE(Player);

Player::Player(Creature* c, Model* m, bool greet, map<Level*, MapMemory>* memory) :
    Controller(c), levelMemory(memory), model(m), displayGreeting(greet) {
}

Player::~Player() {
}

const Level* Player::getListenerLevel() const {
  return creature->getLevel();
}

void Player::onThrowEvent(const Creature* thrower, const Item* item, const vector<Vec2>& trajectory) {
  for (Vec2 v : trajectory)
    if (creature->canSee(v)) {
      model->getView()->animateObject(trajectory, item->getViewObject());
      return;
    }
}

void Player::learnLocation(const Location* loc) {
  for (Vec2 v : loc->getBounds())
    (*levelMemory)[creature->getLevel()].addObject(v, creature->getLevel()->getSquare(v)->getViewObject());
}

void Player::onExplosionEvent(const Level* level, Vec2 pos) {
  if (creature->canSee(pos))
    model->getView()->animation(pos, AnimationId::EXPLOSION);
  else
    privateMessage("BOOM!");
}

void Player::onAlarmEvent(const Level* l, Vec2 pos) {
  if (pos == creature->getPosition())
    privateMessage("An alarm sounds near you.");
  else
    privateMessage("An alarm sounds in the " + 
        getCardinalName((pos - creature->getPosition()).getBearing().getCardinalDir()));
}

ControllerFactory Player::getFactory(Model *m, map<Level*, MapMemory>* levelMemory) {
  return ControllerFactory([=](Creature* c) { return new Player(c, m, true, levelMemory);});
}

map<EquipmentSlot, string> slotSuffixes = {
    {EquipmentSlot::WEAPON, "(weapon ready)"},
    {EquipmentSlot::RANGED_WEAPON, "(ranged weapon ready)"},
    {EquipmentSlot::BODY_ARMOR, "(being worn)"},
    {EquipmentSlot::HELMET, "(being worn)"},
    {EquipmentSlot::BOOTS, "(being worn)"},
    {EquipmentSlot::AMULET, "(being worn)"}};

void Player::onBump(Creature*) {
  FAIL << "Shouldn't call onBump on a player";
}

void Player::getItemNames(vector<Item*> items, vector<View::ListElem>& names, vector<vector<Item*> >& groups,
    ItemPredicate predicate) {
  map<string, vector<Item*> > ret = groupBy<Item*, string>(items, 
      [this] (Item* const& item) { 
        if (creature->getEquipment().isEquiped(item))
          return item->getNameAndModifiers(false, creature->isBlind()) + " " 
              + slotSuffixes[creature->getEquipment().getSlot(item)];
        else
          return item->getNameAndModifiers(false, creature->isBlind());});
  for (auto elem : ret) {
    if (elem.second.size() == 1)
      names.emplace_back(elem.first, predicate(elem.second[0]) ? View::NORMAL : View::INACTIVE);
    else
      names.emplace_back(convertToString<int>(elem.second.size()) + " " 
          + elem.second[0]->getNameAndModifiers(true, creature->isBlind()),
          predicate(elem.second[0]) ? View::NORMAL : View::INACTIVE);
    groups.push_back(elem.second);
  }
}

static string getSquareQuestion(SquareApplyType type, string name) {
  switch (type) {
    case SquareApplyType::DESCEND: return "Descend the " + name;
    case SquareApplyType::ASCEND: return "Ascend the " + name;
    case SquareApplyType::USE_CHEST: return "Open the " + name;
    case SquareApplyType::DRINK: return "Drink from the " + name;
    case SquareApplyType::PRAY: return "Pray at the " + name;
    case SquareApplyType::SLEEP: return "Go to sleep on the " + name;
    default: break;
  }
  return "";
}

void Player::pickUpAction(bool extended) {
  auto items = creature->getPickUpOptions();
  const Square* square = creature->getConstSquare();
  if (square->getApplyType(creature)) {
    string question = getSquareQuestion(*square->getApplyType(creature), square->getName());
    if (!question.empty() && (items.empty() || model->getView()->yesOrNoPrompt(question))) {
      creature->applySquare().perform();
      return;
    }
  }
  vector<View::ListElem> names;
  vector<vector<Item*> > groups;
  getItemNames(creature->getPickUpOptions(), names, groups);
  if (names.empty())
    return;
  int index = 0;
  if (names.size() > 1) {
    Optional<int> res = model->getView()->chooseFromList("Choose an item to pick up:", names);
    if (!res)
      return;
    else
      index = *res;
  }
  int num = groups[index].size();
  if (num < 1)
    return;
  if (extended && num > 1) {
    Optional<int> res = model->getView()->getNumber("Pick up how many " + groups[index][0]->getName(true) + "?",
        1, num);
    if (!res)
      return;
    num = *res;
  }
  vector<Item*> pickUpItems = getPrefix(groups[index], 0, num);
  tryToPerform(creature->pickUp(pickUpItems));
}

void Player::tryToPerform(Creature::Action action) {
  if (action)
    action.perform();
  else
    creature->playerMessage(action.getFailedReason());
}

void Player::itemsMessage() {
  vector<View::ListElem> names;
  vector<vector<Item*> > groups;
  getItemNames(creature->getPickUpOptions(), names, groups);
  if (names.size() > 1)
    privateMessage(creature->isBlind() ? "You feel here some items" : "You see here some items.");
  else if (names.size() == 1)
    privateMessage((creature->isBlind() ? string("You feel here ") : ("You see here ")) + 
        (groups[0].size() == 1 ? "a " + groups[0][0]->getNameAndModifiers(false, creature->isBlind()) :
            names[0].getText()));
}

ItemType typeDisplayOrder[] { ItemType::WEAPON, ItemType::RANGED_WEAPON, ItemType::AMMO, ItemType::ARMOR, ItemType::POTION, ItemType::SCROLL, ItemType::FOOD, ItemType::BOOK, ItemType::AMULET, ItemType::TOOL, ItemType::CORPSE, ItemType::OTHER, ItemType::GOLD };

static string getText(ItemType type) {
  switch (type) {
    case ItemType::WEAPON: return "Weapons";
    case ItemType::RANGED_WEAPON: return "Ranged weapons";
    case ItemType::AMMO: return "Projectiles";
    case ItemType::AMULET: return "Amulets";
    case ItemType::ARMOR: return "Armor";
    case ItemType::SCROLL: return "Scrolls";
    case ItemType::POTION: return "Potions";
    case ItemType::FOOD: return "Comestibles";
    case ItemType::BOOK: return "Books";
    case ItemType::TOOL: return "Tools";
    case ItemType::CORPSE: return "Corpses";
    case ItemType::OTHER: return "Other";
    case ItemType::GOLD: return "Gold";
  }
  FAIL << int(type);
  return "";
}


vector<Item*> Player::chooseItem(const string& text, ItemPredicate predicate, Optional<UserInput::Type> exitAction) {
  map<ItemType, vector<Item*> > typeGroups = groupBy<Item*, ItemType>(
      creature->getEquipment().getItems(), [](Item* const& item) { return item->getType();});
  vector<View::ListElem> names;
  vector<vector<Item*> > groups;
  for (auto elem : typeDisplayOrder) 
    if (typeGroups[elem].size() > 0) {
      names.push_back(View::ListElem(getText(elem), View::TITLE));
      getItemNames(typeGroups[elem], names, groups, predicate);
    }
  Optional<int> index = model->getView()->chooseFromList(text, names, 0, View::NORMAL_MENU, nullptr, exitAction);
  if (index)
    return groups[*index];
  return vector<Item*>();
}

void Player::dropAction(bool extended) {
  vector<Item*> items = chooseItem("Choose an item to drop:", [this](const Item* item) {
      return !creature->getEquipment().isEquiped(item) || item->getType() == ItemType::WEAPON;}, UserInput::DROP);
  int num = items.size();
  if (num < 1)
    return;
  if (extended && num > 1) {
    Optional<int> res = model->getView()->getNumber("Drop how many " + items[0]->getName(true, creature->isBlind()) 
        + "?", 1, num);
    if (!res)
      return;
    num = *res;
  }
  tryToPerform(creature->drop(getPrefix(items, 0, num)));
}

void Player::onItemsAppeared(vector<Item*> items, const Creature* from) {
  if (!creature->pickUp(items))
    return;
  vector<View::ListElem> names;
  vector<vector<Item*> > groups;
  getItemNames(items, names, groups);
  CHECK(!names.empty());
  Optional<int> index = model->getView()->chooseFromList("Do you want to take this item?", names);
  if (!index) {
    return;
  }
  int num = groups[*index].size(); //groups[index].size() == 1 ? 1 : howMany(model->getView(), groups[index].size());
  if (num < 1)
    return;
  privateMessage("You take " + creature->getPluralName(groups[*index][0], num));
  tryToPerform(creature->pickUp(getPrefix(groups[*index], 0, num), false));
}

void Player::applyAction() {
  vector<Item*> items = chooseItem("Choose an item to apply:", [this](const Item* item) {
      return creature->applyItem(const_cast<Item*>(item));}, UserInput::APPLY_ITEM);
  if (items.size() == 0)
    return;
  applyItem(items);
}

void Player::applyItem(vector<Item*> items) {
  if (creature->isBlind() && contains({ItemType::SCROLL, ItemType::BOOK}, items[0]->getType())) {
    privateMessage("You can't read while blind!");
    return;
  }
  if (items[0]->getApplyTime() > 1) {
    for (const Creature* c : creature->getVisibleEnemies())
      if ((c->getPosition() - creature->getPosition()).length8() < 3) { 
        if (!model->getView()->yesOrNoPrompt("Applying " + items[0]->getAName() + " takes " + 
            convertToString(items[0]->getApplyTime()) + " turns. Are you sure you want to continue?"))
          return;
        else
          break;
      }
  }
  tryToPerform(creature->applyItem(items[0]));
}

void Player::throwAction(Optional<Vec2> dir) {
  vector<Item*> items = chooseItem("Choose an item to throw:", [this](const Item* item) {
      return !creature->getEquipment().isEquiped(item);}, UserInput::THROW);
  if (items.size() == 0)
    return;
  throwItem(items, dir);
}

void Player::throwItem(vector<Item*> items, Optional<Vec2> dir) {
  if (items[0]->getType() == ItemType::AMMO && Options::getValue(OptionId::HINTS))
    privateMessage(MessageBuffer::important("To fire arrows equip a bow and use alt + direction key"));
  if (!dir) {
    auto cDir = model->getView()->chooseDirection("Which direction do you want to throw?");
    if (!cDir)
      return;
    dir = *cDir;
  }
  tryToPerform(creature->throwItem(items[0], *dir));
}

void Player::equipmentAction() {
  if (!creature->isHumanoid()) {
    privateMessage("You can't use any equipment.");
    return;
  }
  vector<EquipmentSlot> slots;
  for (auto slot : Equipment::slotTitles)
    slots.push_back(slot.first);
  int index = 0;
  creature->startEquipChain();
  while (1) {
    vector<View::ListElem> list;
    for (auto slot : slots) {
      list.push_back(View::ListElem(Equipment::slotTitles.at(slot), View::TITLE));
      Item* item = creature->getEquipment().getItem(slot);
      if (item)
        list.push_back(item->getNameAndModifiers());
      else
        list.push_back("[Nothing]");
    }
    model->getView()->updateView(creature);
    Optional<int> newIndex = model->getView()->chooseFromList("Equipment", list, index, View::NORMAL_MENU, nullptr,
        UserInput::Type::EQUIPMENT);
    if (!newIndex) {
      creature->finishEquipChain();
      return;
    }
    index = *newIndex;
    EquipmentSlot slot = slots[index];
    if (Item* item = creature->getEquipment().getItem(slot)) {
      tryToPerform(creature->unequip(item));
    } else {
      vector<Item*> items = chooseItem("Choose an item to equip:", [=](const Item* item) {
          return item->canEquip()
          && !creature->getEquipment().isEquiped(item)
          && item->getEquipmentSlot() == slot;});
      if (items.size() == 0) {
        continue;
      }
      if (slot == EquipmentSlot::WEAPON && creature->getAttr(AttrType::STRENGTH) < items[0]->getMinStrength()
          && !model->getView()->yesOrNoPrompt(items[0]->getTheName() + " is too heavy for you, and you will get an accuracy penaulty.\n Do you want to equip it?")) {
        creature->finishEquipChain();
        return;
      }
      tryToPerform(creature->equip(items[0]));
    }
  }
}

void Player::grantIdentify(int numItems) {
  auto unidentFun = [this](const Item* item) { return item->canIdentify() && !item->isIdentified();};
  vector<Item*> unIded = creature->getEquipment().getItems(unidentFun);
  if (unIded.empty()) {
    privateMessage("All your posessions are already identified");
    return;
  }
  if (numItems > unIded.size()) {
    privateMessage("You identify all your posessions");
    for (Item* it : unIded)
      it->identify();
  } else
  for (int i : Range(numItems)) {
    vector<Item*> items = chooseItem("Choose an item to identify:", unidentFun);
    if (items.size() == 0)
      return; 
    items[0]->identify();
    privateMessage("You identify " + items[0]->getTheName());
  }
}

void Player::displayInventory() {
  if (!creature->isHumanoid()) {
    model->getView()->presentText("", "You can't use inventory.");
    return;
  }
  if (creature->getEquipment().isEmpty()) {
    model->getView()->presentText("", "Your inventory is empty.");
    return;
  }
  vector<Item*> item = chooseItem("Inventory:", alwaysTrue<const Item*>(), UserInput::Type::SHOW_INVENTORY);
  if (item.size() == 0) {
    return;
  }
  vector<View::ListElem> options;
  if (creature->equip(item[0])) {
    options.push_back("equip");
  }
  if (creature->applyItem(item[0])) {
    options.push_back("apply");
  }
  if (creature->unequip(item[0]))
    options.push_back("remove");
  else {
    options.push_back("throw");
    options.push_back("drop");
  }
  auto index = model->getView()->chooseFromList("What to do with "
      + creature->getPluralName(item[0], item.size()) + "?", options);
  if (!index) {
    displayInventory();
    return;
  }
  if (options[*index].getText() == "drop")
    tryToPerform(creature->drop(item));
  if (options[*index].getText() == "throw")
    throwItem(item);
  if (options[*index].getText() == "apply")
    applyItem(item);
  if (options[*index].getText() == "remove")
    tryToPerform(creature->unequip(getOnlyElement(item)));
  if (options[*index].getText() == "equip")
    tryToPerform(creature->equip(item[0]));
}

void Player::hideAction() {
  tryToPerform(creature->hide());
}

bool Player::interruptedByEnemy() {
  vector<const Creature*> enemies = creature->getVisibleEnemies();
  vector<string> ignoreCreatures { "a boar" ,"a deer", "a fox", "a vulture", "a rat", "a jackal", "a boulder" };
  if (enemies.size() > 0) {
    for (const Creature* c : enemies)
      if (!contains(ignoreCreatures, c->getAName())) {
        model->getView()->refreshView(creature);
        privateMessage("You notice " + c->getAName());
        return true;
      }
  }
  return false;
}

void Player::travelAction() {
  if (!creature->move(travelDir) || model->getView()->travelInterrupt() || interruptedByEnemy()) {
    travelling = false;
    return;
  }
  tryToPerform(creature->move(travelDir));
  itemsMessage();
  const Location* currentLocation = creature->getLevel()->getLocation(creature->getPosition());
  if (lastLocation != currentLocation && currentLocation != nullptr && currentLocation->hasName()) {
    privateMessage("You arrive at " + addAParticle(currentLocation->getName()));
    travelling = false;
    return;
  }
  vector<Vec2> squareDirs = creature->getConstSquare()->getTravelDir();
  if (squareDirs.size() != 2) {
    travelling = false;
    Debug() << "Stopped by multiple routes";
    return;
  }
  Optional<int> myIndex = findElement(squareDirs, -travelDir);
  CHECK(myIndex) << "Bad travel data in square";
  travelDir = squareDirs[(*myIndex + 1) % 2];
}

void Player::targetAction() {
  CHECK(target);
  if (creature->getPosition() == *target || model->getView()->travelInterrupt()) {
    target = Nothing();
    return;
  }
  if (auto action = creature->moveTowards(*target))
    action.perform();
  else
    target = Nothing();
  itemsMessage();
  if (interruptedByEnemy())
    target = Nothing();
}

void Player::payDebtAction() {
  for (Vec2 v : Vec2::directions8())
    if (const Creature* c = creature->getConstSquare(v)->getCreature()) {
      if (int debt = c->getDebt(creature)) {
        vector<Item*> gold = creature->getGold(debt);
        if (gold.size() < debt) {
          privateMessage("You don't have enough gold to pay.");
        } else if (model->getView()->yesOrNoPrompt("Buy items for " + convertToString(debt) + " zorkmids?")) {
          privateMessage("You pay " + c->getName() + " " + convertToString(debt) + " zorkmids.");
          creature->give(c, gold);
        }
      } else {
        Debug() << "No debt " << c->getName();
      }
    }
}

void Player::chatAction(Optional<Vec2> dir) {
  vector<const Creature*> creatures;
  for (Vec2 v : Vec2::directions8())
    if (const Creature* c = creature->getConstSquare(v)->getCreature())
      creatures.push_back(c);
  if (creatures.size() == 1 && !dir) {
    tryToPerform(creature->chatTo(creatures[0]->getPosition() - creature->getPosition()));
  } else
  if (creatures.size() > 1 || dir) {
    if (!dir)
      dir = model->getView()->chooseDirection("Which direction?");
    if (!dir)
      return;
    tryToPerform(creature->chatTo(*dir));
  }
}

void Player::fireAction(Vec2 dir) {
  tryToPerform(creature->fire(dir));
}

void Player::spellAction() {
  vector<View::ListElem> list;
  auto spells = creature->getSpells();
  for (int i : All(spells))
    list.push_back(View::ListElem(spells[i].name + " " + (!creature->castSpell(i) ? "(ready in " +
          convertToString(int(spells[i].ready - creature->getTime() + 0.9999)) + " turns)" : ""),
          creature->castSpell(i) ? View::NORMAL : View::INACTIVE));
  auto index = model->getView()->chooseFromList("Cast a spell:", list);
  if (!index)
    return;
  tryToPerform(creature->castSpell(*index));
}

const MapMemory& Player::getMemory() const {
  return (*levelMemory)[creature->getLevel()];
}

void Player::sleeping() {
  if (creature->isAffected(LastingEffect::HALLU))
    ViewObject::setHallu(true);
  else
    ViewObject::setHallu(false);
  MEASURE(
      model->getView()->refreshView(creature),
      "level render time");
}

static bool displayTravelInfo = true;

void Player::attackAction(Creature* other) {
  vector<View::ListElem> elems;
  vector<AttackLevel> levels = creature->getAttackLevels();
  for (auto level : levels)
    switch (level) {
      case AttackLevel::LOW: elems.push_back("Low"); break;
      case AttackLevel::MIDDLE: elems.push_back("Middle"); break;
      case AttackLevel::HIGH: elems.push_back("High"); break;
    }
  if (auto ind = model->getView()->chooseFromList("Choose level of the attack:", elems))
    creature->attack(other, levels[*ind]).perform();;
}

void Player::makeMove() {
  vector<Vec2> squareDirs = creature->getConstSquare()->getTravelDir();
  const vector<Creature*>& creatures = creature->getLevel()->getAllCreatures();
  if (creature->isAffected(LastingEffect::HALLU))
    ViewObject::setHallu(true);
  else
    ViewObject::setHallu(false);
  MEASURE(
      model->getView()->refreshView(creature),
      "level render time");
  if (Options::getValue(OptionId::HINTS) && displayTravelInfo && creature->getConstSquare()->getName() == "road") {
    model->getView()->presentText("", "Use ctrl + arrows to travel quickly on roads and corridors.");
    displayTravelInfo = false;
  }
  static bool greeting = false;
  if (Options::getValue(OptionId::HINTS) && displayGreeting) {
    CHECK(creature->getFirstName());
    model->getView()->presentText("", "Dear " + *creature->getFirstName() + ",\n \n \tIf you are reading this letter, then you have arrived in the valley of " + NameGenerator::worldNames.getNext() + ". There is a band of dwarves dwelling in caves under a mountain. Find them, talk to them, they will help you. Let your sword guide you.\n \n \nYours, " + NameGenerator::firstNames.getNext() + "\n \nPS.: Beware the goblins!");
    model->getView()->presentText("", "Every settlement that you find has a leader, and they may have quests for you."
        "\n \nYou can turn these messages off in the options (press F2).");
    displayGreeting = false;
    model->getView()->refreshView(creature);
  }
  for (const Creature* c : creature->getVisibleEnemies()) {
    if (c->isSpecialMonster() && !contains(specialCreatures, c)) {
      privateMessage(MessageBuffer::important(c->getDescription()));
      model->getView()->refreshView(creature);
      specialCreatures.push_back(c);
    }
  }
  if (travelling)
    travelAction();
  else if (target)
    targetAction();
  else {
    UserInput action = model->getView()->getAction();
    Debug() << "Action " << int(action.type);
  vector<Vec2> direction;
  bool travel = false;
  if (action.type != UserInput::IDLE)
    model->getView()->retireMessages();
  switch (action.type) {
    case UserInput::FIRE: fireAction(action.getPosition()); break;
    case UserInput::TRAVEL: travel = true;
    case UserInput::MOVE: direction.push_back(action.getPosition()); break;
    case UserInput::MOVE_TO: 
      if (action.getPosition().dist8(creature->getPosition()) == 1) {
        Vec2 dir = action.getPosition() - creature->getPosition();
        if (const Creature* c = creature->getConstSquare(dir)->getCreature()) {
          if (!creature->isEnemy(c)) {
            chatAction(dir);
            break;
          }
        }
        direction.push_back(dir);
      } else
        if (action.getPosition() != creature->getPosition()) {
          target = action.getPosition();
          target = Vec2(min(creature->getLevel()->getBounds().getKX() - 1, max(0, target->x)),
              min(creature->getLevel()->getBounds().getKY() - 1, max(0, target->y)));
          // Just in case
          if (!target->inRectangle(creature->getLevel()->getBounds()))
            target = Nothing();
        }
        else
          pickUpAction(false);
      break;
    case UserInput::SHOW_INVENTORY: displayInventory(); break;
    case UserInput::PICK_UP: pickUpAction(false); break;
    case UserInput::EXT_PICK_UP: pickUpAction(true); break;
    case UserInput::DROP: dropAction(false); break;
    case UserInput::EXT_DROP: dropAction(true); break;
    case UserInput::WAIT: creature->wait().perform(); break;
    case UserInput::APPLY_ITEM: applyAction(); break; 
    case UserInput::THROW: throwAction(); break;
    case UserInput::THROW_DIR: throwAction(action.getPosition()); break;
    case UserInput::EQUIPMENT: equipmentAction(); break;
    case UserInput::HIDE: hideAction(); break;
    case UserInput::PAY_DEBT: payDebtAction(); break;
    case UserInput::CHAT: chatAction(); break;
    case UserInput::SHOW_HISTORY: messageBuffer.showHistory(); break;
    case UserInput::UNPOSSESS:
      if (unpossess()) {
        creature->popController();
        return;
      }
      break;
    case UserInput::CAST_SPELL: spellAction(); break;
    case UserInput::DRAW_LEVEL_MAP: model->getView()->drawLevelMap(creature); break;
    case UserInput::EXIT: model->exitAction(); break;
    case UserInput::IDLE: break;
    default: break;
  }
  if (creature->isAffected(LastingEffect::SLEEP)) {
    onFellAsleep();
    return;
  }
  for (Vec2 dir : direction)
    if (travel) {
      if (Creature* other = creature->getSquare(dir)->getCreature())
        attackAction(other);
      else {
        vector<Vec2> squareDirs = creature->getConstSquare()->getTravelDir();
        if (findElement(squareDirs, dir)) {
          travelDir = dir;
          lastLocation = creature->getLevel()->getLocation(creature->getPosition());
          travelling = true;
          travelAction();
        }
      }
    } else {
      moveAction(dir);
      break;
    }
  }
  for (Vec2 pos : creature->getLevel()->getVisibleTiles(creature)) {
    (*levelMemory)[creature->getLevel()].update(pos, creature->getLevel()->getSquare(pos)->getViewIndex(creature));
  }
}

void Player::moveAction(Vec2 dir) {
  if (auto action = creature->move(dir)) {
    action.perform();
    itemsMessage();
  } else if (const Creature *c = creature->getConstSquare(dir)->getCreature()) {
    if (auto action = creature->bumpInto(dir))
      action.perform();
    else if (auto action = creature->destroy(dir, Creature::BASH))
      action.perform();
  }
}

bool Player::isPlayer() const {
  return true;
}

void Player::privateMessage(const string& message) const {
  messageBuffer.addMessage(message);
}

void Player::you(const string& param) const {
  privateMessage("You " + param);
}

void Player::you(MsgType type, const string& param) const {
  string msg;
  switch (type) {
    case MsgType::ARE: msg = "You are " + param; break;
    case MsgType::YOUR: msg = "Your " + param; break;
    case MsgType::FEEL: msg = "You feel " + param; break;
    case MsgType::FALL_ASLEEP: msg = "You fall asleep" + (param.size() > 0 ? " on the " + param : "."); break;
    case MsgType::WAKE_UP: msg = "You wake up."; break;
    case MsgType::FALL_APART: msg = "You fall apart."; break;
    case MsgType::FALL: msg = "You fall on the " + param; break;
    case MsgType::DIE: messageBuffer.addMessage("You die!!"); break;
    case MsgType::TELE_DISAPPEAR: msg = "You are standing somewhere else!"; break;
    case MsgType::BLEEDING_STOPS: msg = "Your bleeding stops."; break;
    case MsgType::DIE_OF: msg = "You die" + (param.empty() ? string(".") : " of " + param); break;
    case MsgType::MISS_ATTACK: msg = "You miss " + param; break;
    case MsgType::MISS_THROWN_ITEM: msg = param + " misses you"; break;
    case MsgType::MISS_THROWN_ITEM_PLURAL: msg = param + " miss you"; break;
    case MsgType::HIT_THROWN_ITEM: msg = param + " hits you"; break;
    case MsgType::HIT_THROWN_ITEM_PLURAL: msg = param + " hit you"; break;
    case MsgType::ITEM_CRASHES: msg = param + " crashes on you."; break;
    case MsgType::ITEM_CRASHES_PLURAL: msg = param + " crash on you."; break;
    case MsgType::GET_HIT_NODAMAGE: msg = "The " + param + " is harmless."; break;
    case MsgType::COLLAPSE: msg = "You collapse."; break;
    case MsgType::TRIGGER_TRAP: msg = "You trigger something."; break;
    case MsgType::SWING_WEAPON: msg = "You swing your " + param; break;
    case MsgType::THRUST_WEAPON: msg = "You thrust your " + param; break;
    case MsgType::ATTACK_SURPRISE: msg = "You sneak attack " + param; break;
    case MsgType::KICK: msg = "You kick " + param; break;
    case MsgType::BITE: msg = "You bite " + param; break;
    case MsgType::PUNCH: msg = "You punch " + param; break;
    case MsgType::PANIC:
          msg = !creature->isAffected(LastingEffect::HALLU) ? "You are suddenly very afraid" : "You freak out completely"; break;
    case MsgType::RAGE:
          msg = !creature->isAffected(LastingEffect::HALLU) ?"You are suddenly very angry" : "This will be a very long trip."; break;
    case MsgType::CRAWL: msg = "You are crawling"; break;
    case MsgType::STAND_UP: msg = "You are back on your feet"; break;
    case MsgType::CAN_SEE_HIDING: msg = param + " can see you hiding"; break;
    case MsgType::TURN_INVISIBLE: msg = "You can see through yourself!"; break;
    case MsgType::TURN_VISIBLE: msg = "You are no longer invisible"; break;
    case MsgType::DROP_WEAPON: msg = "You drop your " + param; break;
    case MsgType::ENTER_PORTAL: msg = "You enter the portal"; break;
    case MsgType::HAPPENS_TO: msg = param + " you."; break;
    case MsgType::BURN: msg = "You burn in the " + param; break;
    case MsgType::DROWN: msg = "You drown in the " + param; break;
    case MsgType::SET_UP_TRAP: msg = "You set up the trap"; break;
    case MsgType::KILLED_BY: msg = "You are killed by " + param; break;
    case MsgType::TURN: msg = "You turn into " + param; break;
    case MsgType::BREAK_FREE: msg = "You break free from " + param; break;
    case MsgType::HIT: msg = "You hit " + param; break;
    default: break;
  }
  messageBuffer.addMessage(msg);
}


void Player::onKilled(const Creature* attacker) {
  if (model->getView()->yesOrNoPrompt("Would you like to see the last messages?"))
    messageBuffer.showHistory();
  model->gameOver(creature, creature->getKills().size(), "monsters", creature->getPoints());
}

bool Player::unpossess() {
  return false;
}

void Player::onFellAsleep() {
}

class PossessedController : public Player {
  public:
  PossessedController(Creature* c, Creature* _owner, Model* m, map<Level*, MapMemory>* memory, bool ghost)
    : Player(c, m, false, memory), owner(_owner), isGhost(ghost) {}

  void onKilled(const Creature* attacker) override {
    if (attacker)
      owner->popController();
  }

  void onAttackEvent(const Creature* victim, const Creature* attacker) override {
    if (!creature->isDead() && victim == owner)
      unpossess();
  }

  bool unpossess() override {
    owner->popController();
    if (isGhost) {
      creature->die();
      return false;
    } else
      return true;
  }

  void moveAction(Vec2 dir) override {
    if (!isGhost) {
      Player::moveAction(dir);
      return;
    }
    if (Creature *c = creature->getSquare(dir)->getCreature()) {
      if (c == owner)
        owner->popController();
      else
        c->pushController(PController(new PossessedController(c, owner, model, levelMemory, false)));
      creature->die();
    } else Player::moveAction(dir);
  }

  void onFellAsleep() override {
    creature->die();
    owner->popController();
  }

  template <class Archive>
  void serialize(Archive& ar, const unsigned int version) {
    ar& SUBCLASS(Player)
      & SVAR(owner)
    CHECK_SERIAL;
  }

  SERIALIZATION_CONSTRUCTOR(PossessedController);

  private:
  Creature* SERIAL(owner);
  bool SERIAL(isGhost);
};

Controller* Player::getPossessedController(Creature* c) {
  creature->pushController(PController(new DoNothingController(creature)));
  return new PossessedController(c, creature, model, levelMemory, true);
}

template <class Archive>
void Player::registerTypes(Archive& ar) {
  REGISTER_TYPE(ar, PossessedController);
}

REGISTER_TYPES(Player);

