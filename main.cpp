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

struct IsVespeneGeyser {
	bool operator()(const Unit& unit) {
		switch (unit.unit_type.ToType()) {
		case UNIT_TYPEID::NEUTRAL_VESPENEGEYSER: return true;
		case UNIT_TYPEID::NEUTRAL_SPACEPLATFORMGEYSER: return true;
		case UNIT_TYPEID::NEUTRAL_PROTOSSVESPENEGEYSER: return true;
		default: return false;
		}
	}
};

struct IsArmy {
	IsArmy(const ObservationInterface* obs) : observation_(obs) {}

	bool operator()(const Unit& unit) {
		auto attributes = observation_->GetUnitTypeData().at(unit.unit_type).attributes;
		for (const auto& attribute : attributes) {
			if (attribute == Attribute::Structure) {
				return false;
			}
		}
		switch (unit.unit_type.ToType()) {
		case UNIT_TYPEID::TERRAN_SCV: return false;
		case UNIT_TYPEID::TERRAN_MULE: return false;
		case UNIT_TYPEID::TERRAN_NUKE: return false;
		default: return true;
		}
	}

	const ObservationInterface* observation_;
};

class Bot : public Agent {
public:
	vector<Tag> occupied_mineral;
	vector<Tag>::iterator it;
	virtual void OnGameStart() final {
		std::cout << "Hello, World!" << std::endl;
	}

	virtual void OnStep() final {
		const ObservationInterface* observation = Observation();
		Units bases = observation->GetUnits(Unit::Alliance::Self, IsTownHall());
		std::cout << Observation()->GetGameLoop() << std::endl;
		TryBuildRefinery();
		TryBuildSupplyDepot();
		TryBuildBarracks();
		if (observation->GetMinerals() > 400) {
			TryBuildCommandCenters();
		}
		
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
			// if there is a base with less than ideal workers
			if (unit->assigned_harvesters < unit->ideal_harvesters && unit->build_progress == 1) {
				if (Observation()->GetMinerals() >= 50) {
					Actions()->UnitCommand(unit, ABILITY_ID::TRAIN_SCV);
				}
			}
		}
		case UNIT_TYPEID::TERRAN_SCV: {
			MineIdleWorkers(unit, ABILITY_ID::HARVEST_GATHER, UNIT_TYPEID::TERRAN_REFINERY);
			break;
		}
		case UNIT_TYPEID::TERRAN_BARRACKS: {
			if (Observation()->GetGameLoop() % 2 == 0) {
				Actions()->UnitCommand(unit, ABILITY_ID::BUILD_TECHLAB_BARRACKS);
			} else {
				Actions()->UnitCommand(unit, ABILITY_ID::BUILD_REACTOR_BARRACKS);
			}
			if (Observation()->GetGameLoop() % 3 == 0) {
				Actions()->UnitCommand(unit, ABILITY_ID::TRAIN_MARAUDER);
			}
			if (Observation()->GetGameLoop() % 3 == 1) {
				Actions()->UnitCommand(unit, ABILITY_ID::TRAIN_MARINE);
			}
			if (Observation()->GetGameLoop() % 3 == 2) {
				Actions()->UnitCommand(unit, ABILITY_ID::TRAIN_REAPER);
			}
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
		case UNIT_TYPEID::TERRAN_ENGINEERINGBAY: {
			Actions()->UnitCommand(unit, ABILITY_ID::RESEARCH_TERRANINFANTRYWEAPONS);
			Actions()->UnitCommand(unit, ABILITY_ID::RESEARCH_TERRANINFANTRYARMOR);
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

	// from s2client-api\examples\common\bot_examples.cc line 359
    // Try to build a structure based on tag, Used mostly for Vespene, since the pathing check will fail even though the geyser is "Pathable"
	bool TryBuildStructure(AbilityID ability_type_for_structure, UnitTypeID unit_type, Tag location_tag) {
		const ObservationInterface* observation = Observation();
		Units workers = observation->GetUnits(Unit::Alliance::Self, IsUnit(unit_type));
		const Unit* target = observation->GetUnit(location_tag);

		if (workers.empty()) {
			return false;
		}

		// Check to see if there is already a worker heading out to build it
		for (const auto& worker : workers) {
			for (const auto& order : worker->orders) {
				if (order.ability_id == ability_type_for_structure) {
					return false;
				}
			}
		}

		// If no worker is already building one, get a random worker to build one
		const Unit* unit = GetRandomEntry(workers);

		// Check to see if unit can build there
		if (Query()->Placement(ability_type_for_structure, target->pos)) {
			Actions()->UnitCommand(unit, ability_type_for_structure, target);
			return true;
		}
		return false;

	}

	// from s2client-api\examples\common\bot_examples.cc line 508
	// An estimate of how many workers we should have based on what buildings we have
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

	void MineIdleWorkers(const Unit* worker, AbilityID worker_gather_command, UnitTypeID vespene_building_type) {
		const ObservationInterface* observation = Observation();
		Units bases = observation->GetUnits(Unit::Alliance::Self, IsTownHall());
		Units geysers = observation->GetUnits(Unit::Alliance::Self, IsUnit(vespene_building_type));

		const Unit* valid_mineral_patch = nullptr;

		if (bases.empty()) {
			return;
		}

		for (const auto& geyser : geysers) {
			if (geyser->assigned_harvesters < geyser->ideal_harvesters) {
				Actions()->UnitCommand(worker, worker_gather_command, geyser);
				return;
			}
		}
		//Search for a base that is missing workers.
		for (const auto& base : bases) {
			//If we have already mined out here skip the base.
			if (base->ideal_harvesters == 0 || base->build_progress != 1) {
				continue;
			}
			if (base->assigned_harvesters < base->ideal_harvesters) {
				valid_mineral_patch = FindNearestMineralPatch(base->pos);
				Actions()->UnitCommand(worker, worker_gather_command, valid_mineral_patch);
				return;
			}
		}

		if (!worker->orders.empty()) {
			return;
		}

		//If all workers are spots are filled just go to any base.
		const Unit* random_base = GetRandomEntry(bases);
		valid_mineral_patch = FindNearestMineralPatch(random_base->pos);
		Actions()->UnitCommand(worker, worker_gather_command, valid_mineral_patch);
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

	// from s2client-api\examples\common\bot_examples.cc line 2904, for build refinery
	bool TryBuildRefinery() {
		const ObservationInterface* observation = Observation();
		Units bases = observation->GetUnits(Unit::Alliance::Self, IsTownHall());

		if (CountUnitType(UNIT_TYPEID::TERRAN_REFINERY) >= observation->GetUnits(Unit::Alliance::Self, IsTownHall()).size() * 2) {
			return false;
		}

		for (const auto& base : bases) {
			if (base->assigned_harvesters >= base->ideal_harvesters) {
				if (base->build_progress == 1) {
					if (TryBuildGas(ABILITY_ID::BUILD_REFINERY, UNIT_TYPEID::TERRAN_SCV, base->pos)) {
						return true;
					}
				}
			}
		}
		return false;
	}

	// from s2client-api\examples\common\bot_examples.cc line 418, tries to build a geyser for a base
	bool TryBuildGas(AbilityID build_ability, UnitTypeID worker_type, Point2D base_location) {
		const ObservationInterface* observation = Observation();
		Units geysers = observation->GetUnits(Unit::Alliance::Neutral, IsVespeneGeyser());

		//only search within this radius
		float minimum_distance = 15.0f;
		Tag closestGeyser = 0;
		for (const auto& geyser : geysers) {
			float current_distance = Distance2D(base_location, geyser->pos);
			if (current_distance < minimum_distance) {
				if (Query()->Placement(build_ability, geyser->pos)) {
					minimum_distance = current_distance;
					closestGeyser = geyser->tag;
				}
			}
		}

		// In the case where there are no more available geysers nearby
		if (closestGeyser == 0) {
			return false;
		}
		return TryBuildStructure(build_ability, worker_type, closestGeyser);

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
				const Unit* nearest = FindNearestCommandCenter(u->pos);
				if (nearest != nullptr) {
					if (Distance2D(u->pos, Point2D(FindNearestCommandCenter(u->pos)->pos.x, FindNearestCommandCenter(u->pos)->pos.y)) > 20.0f) {
						TryBuildStructureAt(u->pos, ABILITY_ID::BUILD_COMMANDCENTER);
					}
				}
				
			}
		}
	}

	bool TryDefense() {
		const ObservationInterface* observation = Observation();
		Units enemies = observation->GetUnits(Unit::Alliance::Enemy);
		Units defenses = observation->GetUnits(Unit::Alliance::Self, IsArmy(observation));
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
		Units defenses = observation->GetUnits(Unit::Alliance::Self, IsArmy(observation));
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