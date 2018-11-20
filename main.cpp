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
    }

    virtual void OnStep() final {
		const ObservationInterface* observation = Observation();
        std::cout << Observation()->GetGameLoop() << std::endl;
		TryBuildSupplyDepot();
		TryBuildBarracks();
		TryBuildStructure(ABILITY_ID::BUILD_BUNKER);
		if (CountUnitType(UNIT_TYPEID::TERRAN_ENGINEERINGBAY) < 1) {
			TryBuildStructure(ABILITY_ID::BUILD_ENGINEERINGBAY);
		}
		if (CountUnitType(UNIT_TYPEID::TERRAN_MISSILETURRET) < (CountUnitType(UNIT_TYPEID::TERRAN_MISSILETURRET) / 3)) {
			TryBuildStructure(ABILITY_ID::BUILD_MISSILETURRET);
		}

    }

	virtual void OnUnitIdle(const Unit* unit) final {
		switch (unit->unit_type.ToType()) {
			case UNIT_TYPEID::TERRAN_COMMANDCENTER: {
				// from s2client-api\examples\common\bot_examples.cc line 2092, for training SCV 
				const ObservationInterface* observation = Observation();
				Units bases = observation->GetUnits(Unit::Alliance::Self, IsTownHall());

				for (const auto& base : bases) {
					//if there is a base with less than ideal workers
					if (base->assigned_harvesters < base->ideal_harvesters && base->build_progress == 1) {
						if (observation->GetMinerals() >= 50) {
							Actions()->UnitCommand(unit, ABILITY_ID::TRAIN_SCV);
						}
					}
				}
			    break;
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
		if (observation->GetFoodUsed() <= observation->GetFoodCap() - 2)
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