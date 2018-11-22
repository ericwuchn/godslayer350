#include <sc2api/sc2_api.h>

#include <iostream>
#include <vector>

using namespace sc2;
using namespace std;

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
	vector<Point2D> occupied_mineral;
	virtual void OnGameStart() final {
		std::cout << "Hello, World!" << std::endl;
	}

	virtual void OnStep() final {
		const ObservationInterface* observation = Observation();
		Units bases = observation->GetUnits(Unit::Alliance::Self, IsTownHall());
		std::cout << Observation()->GetGameLoop() << std::endl;
		TryBuildSupplyDepot();
		TryBuildBarracks();
		TryBuildCommandCenters();
		TryBuildSCV();

		if (CountUnitType(UNIT_TYPEID::TERRAN_BUNKER) < (CountUnitType(UNIT_TYPEID::TERRAN_BARRACKS) / 3)) {
			TryBuildStructure(ABILITY_ID::BUILD_BUNKER);
		}
		if (CountUnitType(UNIT_TYPEID::TERRAN_ENGINEERINGBAY) < 1) {
			TryBuildStructure(ABILITY_ID::BUILD_ENGINEERINGBAY);
		}
		if (CountUnitType(UNIT_TYPEID::TERRAN_MISSILETURRET) < (CountUnitType(UNIT_TYPEID::TERRAN_BUNKER) * 1)) {
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
			//if (unit->assigned_harvesters < unit->ideal_harvesters && unit->build_progress == 1) {
			//	if (Observation()->GetMinerals() >= 50) {
			//		Actions()->UnitCommand(unit, ABILITY_ID::TRAIN_SCV);
			//	}
			//}

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
			//if (Observation()->GetMinerals() >= 50) {
				Actions()->UnitCommand(unit, ABILITY_ID::TRAIN_MARINE);
			//}
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

	// from s2client-api\examples\common\bot_examples.cc line 508
	//An estimate of how many workers we should have based on what buildings we have
	int GetExpectedWorkers(UNIT_TYPEID vespene_building_type) {
		const ObservationInterface* observation = Observation();
		Units bases = observation->GetUnits(Unit::Alliance::Self, IsTownHall());
		Units geysers = observation->GetUnits(Unit::Alliance::Self, IsUnit(vespene_building_type));
		int expected_workers = 0;
		for (const auto& base : bases) {
			if (base->build_progress != 1) {
				continue;
			}
			expected_workers += base->ideal_harvesters;
		}

		for (const auto& geyser : geysers) {
			if (geyser->vespene_contents > 0) {
				if (geyser->build_progress != 1) {
					continue;
				}
				expected_workers += geyser->ideal_harvesters;
			}
		}

		return expected_workers;
	}

	// from s2client-api\examples\common\bot_examples.cc line 2092, for training SCV 
	bool TryBuildSCV() {
		const ObservationInterface* observation = Observation();
		Units bases = observation->GetUnits(Unit::Alliance::Self, IsTownHall());

		for (const auto& base : bases) {
			if (base->unit_type == UNIT_TYPEID::TERRAN_ORBITALCOMMAND && base->energy > 50) {
				if (FindNearestMineralPatch(base->pos)) {
					Actions()->UnitCommand(base, ABILITY_ID::EFFECT_CALLDOWNMULE);
				}
			}
		}

		//if (observation->GetFoodWorkers() >= max_worker_count_) {
		//	return false;
		//}

		if (observation->GetFoodUsed() >= observation->GetFoodCap()) {
			return false;
		}

		if (observation->GetFoodWorkers() > GetExpectedWorkers(UNIT_TYPEID::TERRAN_REFINERY)) {
			return false;
		}

		for (const auto& base : bases) {
			//if there is a base with less than ideal workers
			if (base->assigned_harvesters < base->ideal_harvesters && base->build_progress == 1) {
				if (observation->GetMinerals() >= 50) {
					Actions()->UnitCommand(base, ABILITY_ID::TRAIN_SCV);
					return true;
				}
			}
		}
		return false;
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
		if (CountUnitType(UNIT_TYPEID::TERRAN_BARRACKS) >= 2 * (CountUnitType(UNIT_TYPEID::TERRAN_COMMANDCENTER) + CountUnitType(UNIT_TYPEID::TERRAN_ORBITALCOMMAND) + CountUnitType(UNIT_TYPEID::TERRAN_ORBITALCOMMANDFLYING) + CountUnitType(UNIT_TYPEID::TERRAN_PLANETARYFORTRESS))) {
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

		// from line 380 to check to see if unit can build there
		if (Query()->Placement(ability_type_for_structure, Point2D(position.x + rx * 8.0f, position.y + ry * 8.0f))) {
			Actions()->UnitCommand(unit_to_build,
				ability_type_for_structure,
				Point2D(position.x + rx * 8.0f, position.y + ry * 8.0f));
			return true;
		}
		return false;
	}

	bool TryBuildStructureExactly(Point2D position, ABILITY_ID ability_type_for_structure, UNIT_TYPEID unit_type = UNIT_TYPEID::TERRAN_SCV) {
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

		// from line 380 to check to see if unit can build there
		if (Query()->Placement(ability_type_for_structure, position)) {
			Actions()->UnitCommand(unit_to_build,
				ability_type_for_structure,
				position);
			return true;
		}
		return false;
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
			if (u->unit_type == UNIT_TYPEID::NEUTRAL_MINERALFIELD && Distance2D(u->pos, Observation()->GetStartLocation()) > 20.0f && Distance2D(u->pos, Observation()->GetGameInfo().enemy_start_locations[0]) > 20.0f && Distance2D(u->pos, Observation()->GetGameInfo().enemy_start_locations[1]) > 20.0f && Distance2D(u->pos, Observation()->GetGameInfo().enemy_start_locations[2]) > 20.0f) {
				if (find(occupied_mineral.begin(), occupied_mineral.end(), u->pos) == occupied_mineral.end()) {
					TryBuildStructureAt(u->pos, ABILITY_ID::BUILD_COMMANDCENTER);
					//occupied_mineral.push_back(u->pos);
				}
				
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