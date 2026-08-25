#include "spin360.h"

#include <game/game.h>
#include <cache/cache.h>
#include <sdk/sdk.h>
#include <memory/memory.h>
#include <Offsets/Offsets.hpp>
#include <settings.h>
#include <sdk/math/math.h>

#include <windows.h>
#include <thread>
#include <chrono>
#include <mutex>
#include <cmath>

void rage::spin360::run()
{
	const float TWO_PI = 6.28318530718f;
	float current_angle = 0.0f;
	auto start_time = std::chrono::steady_clock::now();
	bool was_enabled = false;

	bool toggle_state = false;
	bool was_pressed = false;

	while (true)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(5));

		// keybind check
		bool key_active = true;
		int kb = settings::rage::spin360::keybind;
		int kb_mode = settings::rage::spin360::activation_mode;
		if (kb != 0)
		{
			bool is_down = (GetAsyncKeyState(kb) & 0x8000) != 0;
			if (kb_mode == 1) { // Hold
				key_active = is_down;
			} else if (kb_mode == 0) { // Toggle
				if (is_down && !was_pressed) toggle_state = !toggle_state;
				was_pressed = is_down;
				key_active = toggle_state;
			} else { // Always
				key_active = true;
			}
		}

		if (!settings::rage::spin360::enabled || !key_active || !game::datamodel.address)
		{
			// Restore AutoRotate when disabled
			if (was_enabled)
			{
				try
				{
					cache::entity_t lp;
					{
						std::lock_guard<std::mutex> lock(cache::mtx);
						lp = cache::cached_local_player;
					}
					if (lp.humanoid.address != 0)
						memory->write<bool>(lp.humanoid.address + Offsets::Humanoid::AutoRotate, true);
				}
				catch (...) {}
				was_enabled = false;
			}
			current_angle = 0.0f;
			start_time = std::chrono::steady_clock::now();
			continue;
		}

		cache::entity_t local_player;
		{
			std::lock_guard<std::mutex> lock(cache::mtx);
			local_player = cache::cached_local_player;
		}

		if (local_player.humanoid.address == 0)
			continue;

		auto hrp_it = local_player.parts.find("HumanoidRootPart");
		if (hrp_it == local_player.parts.end() || !hrp_it->second.address)
			continue;

		rbx::primitive_t prim = hrp_it->second.get_primitive();
		if (prim.address == 0)
			continue;

		try
		{
			// Disable AutoRotate so Roblox doesn't fight our rotation
			// (this is what makes it work in shiftlock/aiming)
			memory->write<bool>(local_player.humanoid.address + Offsets::Humanoid::AutoRotate, false);
			was_enabled = true;

			// Smooth angle increment based on elapsed time
			// compute angle based on elapsed time from start_time so it is continuous
			auto now = std::chrono::steady_clock::now();
			float total_sec = std::chrono::duration<float>(now - start_time).count();
			if (total_sec < 0.0f) total_sec = 0.0f;

			// Increase speed multiplier so max slider value results in much faster spin
			const float SPEED_MULT = 20.0f;
			current_angle = fmodf(total_sec * settings::rage::spin360::speed * TWO_PI * SPEED_MULT, TWO_PI);

			// Build combined rotation around X/Y/Z so we spin in all directions
			float ax = current_angle;            // X axis angle
			float ay = current_angle * 1.15f;   // Y axis angle (different rate)
			float az = current_angle * 0.9f;    // Z axis angle

			float cx = std::cosf(ax), sx = std::sinf(ax);
			float cy = std::cosf(ay), sy = std::sinf(ay);
			float cz = std::cosf(az), sz = std::sinf(az);

			// Rotation matrices
			math::matrix3 Rx{}; // rotate around X
			Rx.m[0][0] = 1.0f; Rx.m[0][1] = 0.0f; Rx.m[0][2] = 0.0f;
			Rx.m[1][0] = 0.0f; Rx.m[1][1] = cx;   Rx.m[1][2] = -sx;
			Rx.m[2][0] = 0.0f; Rx.m[2][1] = sx;   Rx.m[2][2] = cx;

			math::matrix3 Ry{}; // rotate around Y
			Ry.m[0][0] = cy;   Ry.m[0][1] = 0.0f; Ry.m[0][2] = sy;
			Ry.m[1][0] = 0.0f; Ry.m[1][1] = 1.0f; Ry.m[1][2] = 0.0f;
			Ry.m[2][0] = -sy;  Ry.m[2][1] = 0.0f; Ry.m[2][2] = cy;

			math::matrix3 Rz{}; // rotate around Z
			Rz.m[0][0] = cz;   Rz.m[0][1] = -sz;  Rz.m[0][2] = 0.0f;
			Rz.m[1][0] = sz;   Rz.m[1][1] = cz;   Rz.m[1][2] = 0.0f;
			Rz.m[2][0] = 0.0f; Rz.m[2][1] = 0.0f; Rz.m[2][2] = 1.0f;

			// Multiply R = Rz * Ry * Rx
			auto mul = [](const math::matrix3& A, const math::matrix3& B) {
				math::matrix3 R{};
				for (int r = 0; r < 3; ++r)
					for (int c = 0; c < 3; ++c) {
						float acc = 0.0f;
						for (int k = 0; k < 3; ++k) acc += A.m[r][k] * B.m[k][c];
						R.m[r][c] = acc;
					}
				return R;
			};

			math::matrix3 spin = mul(Rz, mul(Ry, Rx));

			// Write combined rotation
			memory->write<math::matrix3>(prim.address + Offsets::Primitive::Rotation, spin);
		}
		catch (...) {}
	}
}