#include "gravity.h"

#include <game/game.h>
#include <cache/cache.h>
#include <sdk/sdk.h>
#include <memory/memory.h>
#include <Offsets/Offsets.hpp>
#include <settings.h>

#include <thread>
#include <chrono>
#include <menu/keybind/keybind.h>

void movement::gravity::run()
{
	static bool had_override = false;
	static float original_gravity = 0.0f;
	static keybind::keybind_t gravity_kb{};

	while (true)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(5));

		if (game::workspace.address == 0)
			continue;

		try
		{
			if (settings::movement::gravity::enabled)
			{
				// update keybind info
				gravity_kb.key = settings::movement::gravity::keybind;
				gravity_kb.mode = static_cast<keybind::activation_mode>(settings::movement::gravity::activation_mode);

				bool active = keybind::is_active(gravity_kb);

				if (active)
				{
					if (!had_override)
					{
						float current = rbx::humanoid_t::read_gravity();
						// always record original even if zero, so we can restore
						original_gravity = current;
						had_override = true;
					}

					rbx::humanoid_t::write_gravity(settings::movement::gravity::value);
				}
				else
				{
					if (had_override)
					{
						rbx::humanoid_t::write_gravity(original_gravity);
						had_override = false;
					}
				}
			}
			else
			{
				if (had_override)
				{
					rbx::humanoid_t::write_gravity(original_gravity);
					had_override = false;
				}
			}
		}
		catch (...) {}
	}
}

