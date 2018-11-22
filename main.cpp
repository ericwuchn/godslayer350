#include <sc2api/sc2_api.h>

#include <iostream>

using namespace sc2;

// from s2client-api\examples\common\bot_examples.cc line 62, for checking if unit is base
struct IsTownHall {
	bool operator()(const Unit& unit) {
		switch (unit.unit_type.ToType()) {
		case UNIT_TYPEID::ZERG_HATCHERY: return true;
		case UNIT_TYPEID::ZERG_LAIR: return true;
		case UNIT_TYPEID::ZERG_HIVE: return true;
		case UNIT_TYPEID::TERRAN_COMMANDCENTER: return true;
		case UNIT_TYPEID::TERRAN_ORBITALCOMMAND: return true;
		case UNIT_TYPEID::TERRAN_ORBITALCOMMANDFLYING: return true;
		case UNIT_TYPEID::TERRAN_PLANETARYFORTRESS: return true;
		case UNIT_TYPEID::PROTOSS_NEXUS: return true;
		default: return false;
		}
	}
};

class Bot : public Agent {
public:
	virtual void OnGameStart() final {
		std::cout << "Hello, World!" << std::endl;
		game_info = Observation()->GetGameInfo();
	}

	virtual void OnStep() final {
		const ObservationInterface* observation = Observation();
		std::cout << Observation()->GetGameLoop() << std::endl;
		TryBuildSupplyDepot();
		TryBuildBarracks();
		TryBuildCommandCenters();
		TryBuildStructure(ABILITY_ID::BUILD_BUNKER);
		if (CountUnitType(UNIT_TYPEID::TERRAN_ENGINEERINGBAY) < 1) {
			TryBuildStructure(ABILITY_ID::BUILD_ENGINEERINGBAY);
		}
		if (CountUnitType(UNIT_TYPEID::TERRAN_MISSILETURRET) < (CountUnitType(UNIT_TYPEID::TERRAN_BUNKER) / 3)) {
			TryBuildStructure(ABILITY_ID::BUILD_MISSILETURRET);
		}
		TryDefense();
		TryGoBackToCommandCenter();
	}

	virtual void OnUnitIdle(const Unit* unit) final {
		switch (unit->unit_type.ToType()) {
			case UNIT_TYPEID::TERRAN_COMMANDCENTER: {
				// from s2client-api\examples\common\bot_examples.cc line 2092, for training SCV 
				//if there is a base with less than ideal workers
				if (unit->assigned_harvesters < unit->ideal_harvesters && unit->build_progress == 1) {
					if (Observation()->GetMinerals() >= 50) {
						Actions()->UnitCommand(unit, ABILITY_ID::TRAIN_SCV);
					}
				}

			}
			case UNIT_TYPEID::TERRAN_SCV: {
				const Unit* mineral_target = FindNearestMineralPatch(unit->pos);
				if (!mineral_target) {
					break;
				}
				Actions()->UnitCommand(unit, ABILITY_ID::SMART, mineral_target);
				break;
			}
			case UNIT_TYPEID::TERRAN_BARRACKS: {
				Actions()->UnitCommand(unit, ABILITY_ID::TRAIN_MARINE);
				break;
			}
			case UNIT_TYPEID::TERRAN_MARINE: {
				const Unit* bunker_target = FindNearestBunker(unit->pos);
				if (!bunker_target) {
					break;
				}
				Actions()->UnitCommand(unit, ABILITY_ID::SMART, bunker_target);
				break;
			}
			default: {
				break;
			}
		}
	}

private:

	std::vector<const Unit*> army;

	GameInfo game_info;

	size_t CountUnitType(UNIT_TYPEID unit_type) {
		return Observation()->GetUnits(Unit::Alliance::Self, IsUnit(unit_type)).size();
	}

	bool TryBuildStructure(ABILITY_ID ability_type_for_structure, UNIT_TYPEID unit_type = UNIT_TYPEID::TERRAN_SCV) {
		const ObservationInterface* observation = Observation();

		// If a unit already is building a supply structure of this type, do nothing.
		// Also get an scv to build the structure.
		const Unit* unit_to_build = nullptr;
		Units units = observation->GetUnits(Unit::Alliance::Self);
		for (const auto& unit : units) {
			for (const auto& order : unit->orders) {
				if (order.ability_id == ability_type_for_structure) {
					return false;
				}
			}

			if (unit->unit_type == unit_type) {
				unit_to_build = unit;
			}
		}

		float rx = GetRandomScalar();
		float ry = GetRandomScalar();

		Actions()->UnitCommand(unit_to_build,
			ability_type_for_structure,
			Point2D(unit_to_build->pos.x + rx * 15.0f, unit_to_build->pos.y + ry * 15.0f));

		return true;
	}

	const Unit* FindNearestMineralPatch(const Point2D& start) {
		Units units = Observation()->GetUnits(Unit::Alliance::Neutral);
		float distance = std::numeric_limits<float>::max();
		const Unit* target = nullptr;
		for (const auto& u : units) {
			if (u->unit_type == UNIT_TYPEID::NEUTRAL_MINERALFIELD) {
				float d = DistanceSquared2D(u->pos, start);
				if (d < distance) {
					distance = d;
					target = u;
				}
			}
		}
		return target;
	}

	const Unit* FindNearestEnemy(const Point2D& start) {
		Units units = Observation()->GetUnits(Unit::Alliance::Enemy);
		float distance = std::numeric_limits<float>::max();
		const Unit* target = nullptr;
		for (const auto& u : units) {
			float d = DistanceSquared2D(u->pos, start);
			if (d < distance) {
				distance = d;
				target = u;
			}
		}
		return target;
	}

	const Unit* FindNearestCommandCenter(const Point2D& start) {
		Units units = Observation()->GetUnits(Unit::Alliance::Self);
		float distance = std::numeric_limits<float>::max();
		const Unit* target = nullptr;
		for (const auto& u : units) {
			if (u->unit_type == UNIT_TYPEID::TERRAN_COMMANDCENTER) {
				if (u->cargo_space_taken < u->cargo_space_max && u->build_progress == 1.0) {
					float d = DistanceSquared2D(u->pos, start);
					if (d < distance) {
						distance = d;
						target = u;
					}
				}

			}
		}
		return target;
	}

	const Unit* FindNearestBunker(const Point2D& start) {
		Units units = Observation()->GetUnits(Unit::Alliance::Self);
		float distance = std::numeric_limits<float>::max();
		const Unit* target = nullptr;
		for (const auto& u : units) {
			if (u->unit_type == UNIT_TYPEID::TERRAN_BUNKER) {
				if (u->cargo_space_taken < u->cargo_space_max && u->build_progress == 1.0) {
					float d = DistanceSquared2D(u->pos, start);
					if (d < distance) {
						distance = d;
						target = u;
					}
				}

			}
		}
		return target;
	}

	bool TryBuildSupplyDepot() {
		const ObservationInterface* observation = Observation();

		// If we are not supply capped, don't build a supply depot.
		if (observation->GetFoodUsed() <= observation->GetFoodCap() - 4)
			return false;

		// Try and build a depot. Find a random SCV and give it the order.
		return TryBuildStructure(ABILITY_ID::BUILD_SUPPLYDEPOT);
	}

	bool TryBuildBarracks() {
		const ObservationInterface* observation = Observation();

		if (CountUnitType(UNIT_TYPEID::TERRAN_SUPPLYDEPOT) < 1) {
			return false;
		}
		// for every base build 3 barracks
		if (CountUnitType(UNIT_TYPEID::TERRAN_BARRACKS) >= 3 * (CountUnitType(UNIT_TYPEID::TERRAN_COMMANDCENTER) + CountUnitType(UNIT_TYPEID::TERRAN_ORBITALCOMMAND) + CountUnitType(UNIT_TYPEID::TERRAN_ORBITALCOMMANDFLYING) + CountUnitType(UNIT_TYPEID::TERRAN_PLANETARYFORTRESS))) {
			return false;
		}

		return TryBuildStructure(ABILITY_ID::BUILD_BARRACKS);
	}

	bool TryBuildStructureAt(Point2D position, ABILITY_ID ability_type_for_structure, UNIT_TYPEID unit_type = UNIT_TYPEID::TERRAN_SCV) {
		const ObservationInterface* observation = Observation();

		// If a unit already is building a supply structure of this type, do nothing.
		// Also get an scv to build the structure.
		const Unit* unit_to_build = nullptr;
		Units units = observation->GetUnits(Unit::Alliance::Self);
		for (const auto& unit : units) {
			for (const auto& order : unit->orders) {
				if (order.ability_id == ability_type_for_structure) {
					return false;
				}
			}

			if (unit->unit_type == unit_type) {
				unit_to_build = unit;
			}
		}

		float rx = GetRandomScalar();
		float ry = GetRandomScalar();

		Actions()->UnitCommand(unit_to_build,
			ability_type_for_structure,
			Point2D(position.x + rx * 15.0f, position.y + ry * 15.0f));

		return true;
	}

	void TryBuildCommandCenters() {
		Units units = Observation()->GetUnits(Unit::Alliance::Neutral);
		float distance = std::numeric_limits<float>::max();
		const Unit* unit_to_build = nullptr;
		for (const auto& u : units) {
			if (u->unit_type == UNIT_TYPEID::TERRAN_SCV) {
				unit_to_build = u;
			}
		}
		for (const auto& u : units) {
			if (u->unit_type == UNIT_TYPEID::NEUTRAL_MINERALFIELD && Distance2D(u->pos, Observation()->GetStartLocation()) > 50.0f && Distance2D(u->pos, Observation()->GetGameInfo().enemy_start_locations[0]) > 50.0f && Distance2D(u->pos, Observation()->GetGameInfo().enemy_start_locations[1]) > 50.0f && Distance2D(u->pos, Observation()->GetGameInfo().enemy_start_locations[2]) > 50.0f) {
				TryBuildStructureAt(u->pos, ABILITY_ID::BUILD_COMMANDCENTER);

			}
		}
	}

	bool TryDefense() {
		const ObservationInterface* observation = Observation();
		Units enemies = observation->GetUnits(Unit::Alliance::Enemy);
		Units defenses = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_MARINE));
		if (enemies.size() > 0) {
			for (const auto& defense : defenses) {
				Actions()->UnitCommand(defense, ABILITY_ID::ATTACK_ATTACK, FindNearestEnemy(defense->pos));
			}
			return true;
		}
		return false;
	}

	bool TryGoBackToCommandCenter() {
		const ObservationInterface* observation = Observation();
		Units defenses = observation->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_MARINE));
		for (const auto& defense : defenses) {
			if (DistanceSquared2D(defense->pos, FindNearestCommandCenter(defense->pos)->pos) > 500.0f) {
				Actions()->UnitCommand(defense, ABILITY_ID::SMART, FindNearestCommandCenter(defense->pos)->pos);
			}
			return true;
		}
		return false;
	}

	bool FindEnemyPosition(Point2D& target_pos) {
		if (game_info.enemy_start_locations.empty()) {
			return false;
		}
		target_pos = game_info.enemy_start_locations.front();
		return true;
	}

	bool TryAttack() {
		const ObservationInterface* observation = Observation();
		if (army.size() > 50){
			Actions()->UnitCommand(army, ABILITY_ID::ATTACK_ATTACK, game_info.enemy_start_locations.front());
			return true;
		}
		return false;
	}

	bool TryScout() {
		Units units = Observation()->GetUnits(Unit::Alliance::Self);
		for (const auto& unit : units) {
			UnitTypeID unit_type(unit->unit_type);
			if (unit_type != UNIT_TYPEID::TERRAN_MARINE) {
				continue;
			}

			if (!unit->orders.empty()) {
				continue;
			}

			// Priority to attacking enemy structures.
			const Unit* enemy_unit = nullptr;
			Point2D target_pos;
			if (FindEnemyPosition(target_pos)) {
				Actions()->UnitCommand(unit, ABILITY_ID::SMART, target_pos);
			}
		}

	}

	bool TryBuildMarine() {
		return TryBuildUnit(ABILITY_ID::TRAIN_MARINE, UNIT_TYPEID::TERRAN_BARRACKS);
	}

	bool TryBuildUnit(AbilityID ability_type_for_unit, UnitTypeID unit_type) {
		const ObservationInterface* observation = Observation();

		const Unit* unit = nullptr;
		if (!GetRandomUnit(unit, observation, unit_type))
			return false;

		if (!unit->orders.empty()) {
			return false;
		}

		Actions()->UnitCommand(unit, ability_type_for_unit);
		return true;
	}

	bool GetRandomUnit(const Unit*& unit_out, const ObservationInterface* observation, UnitTypeID unit_type) {
		Units my_units = observation->GetUnits(Unit::Alliance::Self);
		std::random_shuffle(my_units.begin(), my_units.end()); // Doesn't work, or doesn't work well.
		for (const auto unit : my_units) {
			if (unit->unit_type == unit_type) {
				unit_out = unit;
				return true;
			}
		}
		return false;
	}

};

int main(int argc, char* argv[]) {
	Coordinator coordinator;
	coordinator.LoadSettings(argc, argv);

	Bot bot;
	coordinator.SetParticipants({
		CreateParticipant(Race::Terran, &bot),
		CreateComputer(Race::Zerg, Difficulty::Hard)
		});

	coordinator.LaunchStarcraft();
	coordinator.StartGame("CactusValleyLE.SC2Map");

	while (coordinator.Update()) {
	}

	return 0;
}