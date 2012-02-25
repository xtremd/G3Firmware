#include "StepperAxis.hh"

StepperAxis::StepperAxis() : interface(0) {
    reset();
}

StepperAxis::StepperAxis(StepperInterface& stepper_interface) : interface(&stepper_interface) {
        reset();
}

void StepperAxis::setStepMultiplier(const int8_t new_multiplier) {
        // step_multiplier = new_multiplier;
        // step_change = direction ? step_multiplier : -step_multiplier;
}


void StepperAxis::setTarget(const int32_t target_in, bool relative) {
        if (relative) {
                delta = target_in;
                target = position + target_in;
        } else {
                delta = target_in - position;
                target = target_in;
        }
        direction = true;
        if (delta != 0) {
                interface->setEnabled(true);
        }
	// step_multiplier = 1;
        if (delta < 0) {
                delta = -delta;
                direction = false;
                step_change = -1;
        } else {
                step_change = 1;
        }
        interface->setDirection(direction);
}

void StepperAxis::setHoming(const bool direction_in) {
        direction = direction_in;
        interface->setDirection(direction);
        interface->setEnabled(true);
        delta = 1;
	step_change = direction ? 1 : -1;
}

void StepperAxis::definePosition(const int32_t position_in) {
        position = position_in;
}

void StepperAxis::enableStepper(bool enable) {
        interface->setEnabled(enable);
}

void StepperAxis::reset() {
        position = 0;
        minimum = 0;
        maximum = 0;
        target = 0;
        counter = 0;
        delta = 0;
        // step_multiplier = 1;
        step_change = 1;
#if defined(SINGLE_SWITCH_ENDSTOPS) && (SINGLE_SWITCH_ENDSTOPS == 1)
        endstop_play = ENDSTOP_DEFAULT_PLAY;
        endstop_status = ESS_UNKNOWN;
#endif //SINGLE_SWITCH_ENDSTOPS
}

bool StepperAxis::checkEndstop(const bool isHoming) {
#if defined(SINGLE_SWITCH_ENDSTOPS) && (SINGLE_SWITCH_ENDSTOPS == 1)
        bool hit_endstop = interface->isAtMinimum();
  // We must move at least ENDSTOP_DEBOUNCE from where we hit the endstop before we declare traveling
        if (hit_endstop || (endstop_status == (direction?ESS_AT_MAXIMUM:ESS_AT_MINIMUM) && (endstop_play < ENDSTOP_DEFAULT_PLAY - ENDSTOP_DEBOUNCE))) {
                hit_endstop = true;
    // Did we *just* hit the endstop?
                if (endstop_status == ESS_TRAVELING || (isHoming && endstop_status == ESS_UNKNOWN)) {
                        endstop_play   = ENDSTOP_DEFAULT_PLAY;
                        if (isHoming?direction:prev_direction)
                                endstop_status = ESS_AT_MAXIMUM;
                        else
                                endstop_status = ESS_AT_MINIMUM;

      // OR, are we traveling away from the endstop we just hit and still have play...
                } else if ((direction && endstop_status != ESS_AT_MAXIMUM) || (!direction && endstop_status != ESS_AT_MINIMUM)) {
                        if (endstop_play > 0) {
                                --endstop_play;
                                hit_endstop = false; // pretend this never happened...
                        } else {
        // we ran out of play, so we must be ramming into the side, switch directions
        // endstop_status = !direction ? ESS_AT_MAXIMUM : ESS_AT_MINIMUM;
        // endstop_play   = ENDSTOP_DEFAULT_PLAY;
                        }
                }
    // otherwise we hit the endstop

    // but if we didn't hit an endstop, clear the status
        } else {
                endstop_status = ESS_TRAVELING;
                if (!isHoming) {
                        endstop_play   = ENDSTOP_DEFAULT_PLAY;
                }
        }
        prev_direction = direction;
        return hit_endstop;
#else
	bool hit_endstop = direction ? interface->isAtMaximum() : interface->isAtMinimum();
        return hit_endstop;
#endif
}

// moved to inline
void StepperAxis::doInterrupt(const int32_t &intervals, const int32_t &step_multiplier) {
	bool hit_endstop = checkEndstop(false);
	
	// make a local copies volatiles
	int32_t counter_local = counter; // we have to push this back after! 
	int32_t position_local = position; // we have to push this back after! 
	const int8_t step_change_local = step_change;
	const int32_t delta_local = delta;
	const int32_t intervals_local = intervals;
	
	if (!hit_endstop) {
		for (int8_t steps = step_multiplier; steps > 0; steps--) {
			counter_local += delta_local;

			if (counter_local >= 0) {
				interface->step(true);
				counter_local -= intervals_local;
				position_local += step_change_local;
				interface->step(false);
			}
		}
	} else {
		// looks like a waste, duplicating like this, but we to keep this loop tight
		for (int8_t steps = step_multiplier; steps > 0; steps--) {
			counter_local += delta_local;

			if (counter_local >= 0) {
				counter_local -= intervals_local;
				position_local += step_change_local;
			}
		}
	}
	
	// push the possibly-changed locals back out
	counter = counter_local;
	position = position_local;
}

bool StepperAxis::doHoming(const int32_t &intervals) {
        if (delta == 0) return false;
        counter += delta;
        if (counter >= 0) {
                counter -= intervals;
                bool hit_endstop = checkEndstop(true);
                if (!hit_endstop) {
                        interface->step(true);
                        interface->step(false);
                } else {
                        return false;
                }
                position += step_change;
        }
        return true;
}