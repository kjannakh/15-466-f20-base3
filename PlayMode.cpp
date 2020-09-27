#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <random>

GLuint level_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > level_meshes(LoadTagDefault, []() -> MeshBuffer const* {
	MeshBuffer const* ret = new MeshBuffer(data_path("level1.pnct"));
	level_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > level_scene(LoadTagDefault, []() -> Scene const* {
	return new Scene(data_path("level1.scene"), [&](Scene& scene, Scene::Transform* transform, std::string const& mesh_name) {
		Mesh const& mesh = level_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable& drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = level_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;

	});
});

Load< Sound::Sample > background_loop_sample(LoadTagDefault, []() -> Sound::Sample const* {
	return new Sound::Sample(data_path("faster-does-it.wav"));
});

Load< Sound::Sample > boiling_water_sample(LoadTagDefault, []() -> Sound::Sample const* {
	return new Sound::Sample(data_path("boiling_water.wav"));
});

Load< Sound::Sample > bell_ding_sample(LoadTagDefault, []() -> Sound::Sample const* {
	return new Sound::Sample(data_path("bell.wav"));
});

void PlayMode::update_camera() {
	camera->transform->position.x = cos(yaw) * camera_dist + player->position.x + transformed_cam_offset.x;
	camera->transform->position.y = sin(yaw) * camera_dist + player->position.y + transformed_cam_offset.y;
	camera->transform->position.z = sin(pitch) * camera_dist + player->position.z + transformed_cam_offset.z;

	//prevent camera clipping
	if (camera->transform->position.z < floor) {
		camera->transform->position.z = floor;
		limit_pitch = true;
	}

	if (camera->transform->position.x < game_area.x_min)
		camera->transform->position.x = game_area.x_min;
	if (camera->transform->position.x > game_area.x_max)
		camera->transform->position.x = game_area.x_max;
	if (camera->transform->position.y < game_area.y_min)
		camera->transform->position.y = game_area.y_min;
	if (camera->transform->position.y > game_area.y_max)
		camera->transform->position.y = game_area.y_max;

	// camera looking code sourced from https://learnopengl.com/Getting-started/Camera and https://gamedev.stackexchange.com/questions/149006/direction-vector-to-quaternion
	glm::mat4 view = glm::lookAt(camera->transform->position, player->position + transformed_cam_offset, camera_up);
	camera->transform->rotation = glm::conjugate(glm::quat_cast(view));
}

bool PlayMode::player_at_spot(glm::vec3 spot) {
	float dx = abs(player->position.x - spot.x);
	float dy = abs(player->position.y - spot.y);
	return (dx < spot.z && dy < spot.z);
}

int PlayMode::player_at_rice_cooker() {
	if (player_at_spot(rice_cookers[0])) return 0;
	else if (player_at_spot(rice_cookers[1])) return 1;
	else if (player_at_spot(rice_cookers[2])) return 2;
	return -1;
}

int PlayMode::get_available_bowl() {
	Scene::Transform* bowl;
	for (int i = 0; i < 3; i++) {
		bowl = bowls[i];
		if (bowl->position == off_screen)
			return i;
	}
	return -1;
}

int PlayMode::get_available_bowl_uncooked() {
	Scene::Transform* bowl;
	for (int i = 0; i < 3; i++) {
		bowl = bowl_uncooked_rices[i];
		if (bowl->position == off_screen)
			return i;
	}
	return -1;
}

int PlayMode::get_available_bowl_cooked() {
	Scene::Transform* bowl;
	for (int i = 0; i < 3; i++) {
		bowl = bowl_cooked_rices[i];
		if (bowl->position == off_screen)
			return i;
	}
	return -1;
}

int PlayMode::get_available_bowl_burnt() {
	Scene::Transform* bowl;
	for (int i = 0; i < 3; i++) {
		bowl = bowl_burnt_rices[i];
		if (bowl->position == off_screen)
			return i;
	}
	return -1;
}

void PlayMode::spawn_new_ticket() {
	if (ticket_spawn == num_tickets) return;
	if (mt() < mt.max() / 2) {
		tickets[ticket_spawn].type = RICE;
	}
	else {
		tickets[ticket_spawn].type = RICE;
	}
	tickets[ticket_spawn].timer = 0.0f;
	tickets[ticket_spawn].duration = ticket_duration_min + (mt() / float(mt.max())) * ticket_duration_range;
	ticket_spawn++;
	if (ticket_spawn == num_tickets) {
		for (Ticket ticket : tickets) {
			if (ticket.type == TICKET_NONE) {
				ticket_spawn = 0;
			}
		}
	}
	bell_ding = Sound::play(*bell_ding_sample, 0.5f);
}

void PlayMode::complete_ticket(Ticket_type type) {
	float min_time = 100.0f;
	int min_ticket = -1;
	for (int i = 0; i < num_tickets; i++) {
		Ticket ticket = tickets[i];
		if (ticket.type == type) {
			float time_left = ticket.duration - ticket.timer;
			if (time_left < min_time) {
				min_time = time_left;
				min_ticket = i;
			}
		}
	}
	if (min_ticket == -1) return;
	tickets[min_ticket].type = TICKET_NONE;
	score += min_time + 1.0f;
}

void PlayMode::update_tickets(float elapsed) {
	for (int i = 0; i < num_tickets; i++) {
		Ticket *ticket = tickets + i;
		if (ticket->type != TICKET_NONE) {
			ticket->timer += elapsed;
			if (ticket->timer > ticket->duration) {
				score -= ticket->duration / 4.0f;
				if (score < 0) score = 0.0f;
				ticket->timer = 0.0f;
				ticket->duration = ticket_duration_min + (mt() / float(mt.max())) * ticket_duration_range;
			}
		}
	}
}

void PlayMode::update_rice_cookers(float elapsed) {
	for (int i = 0; i < 3; i++) {
		if (rice_states[i].state == UNREADY || rice_states[i].state == READY) {
			rice_states[i].timer += elapsed; if (bowl_burnt_rices[0] == nullptr) throw std::runtime_error("GameObject not found.");
			if (rice_states[i].timer > rice_min_duration && rice_states[i].state == UNREADY) {
				rice_states[i].state = READY;
			}
			else if (rice_states[i].timer > rice_max_duration) {
				rice_states[i].state = BURNT;
				rice_states[i].timer = 0.0f;
			}
		}
	}
}

void PlayMode::reset_game() {
	//reset player position
	player->position = glm::vec3(0.0f, 0.0f, 0.0f);
	player->rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
	pitch = 0.0f;
	yaw = 0.0f;
	player_yaw = 0.0f;

	update_camera();
}


PlayMode::PlayMode() : scene(*level_scene) {
	for (auto& transform : scene.transforms) {
		if (transform.name == "Player") player = &transform;
		else if (transform.name == "EmptyBowl") bowls[0] = &transform;
		else if (transform.name == "EmptyBowl1") bowls[1] = &transform;
		else if (transform.name == "EmptyBowl2") bowls[2] = &transform;
		else if (transform.name == "BowlUncookedRice") bowl_uncooked_rices[0] = &transform;
		else if (transform.name == "BowlUncookedRice1") bowl_uncooked_rices[1] = &transform;
		else if (transform.name == "BowlUncookedRice2") bowl_uncooked_rices[2] = &transform;
		else if (transform.name == "BowlCookedRice") bowl_cooked_rices[0] = &transform;
		else if (transform.name == "BowlCookedRice1") bowl_cooked_rices[1] = &transform;
		else if (transform.name == "BowlCookedRice2") bowl_cooked_rices[2] = &transform;
		else if (transform.name == "BowlBurntRice") bowl_burnt_rices[0] = &transform;
		else if (transform.name == "BowlBurntRice1") bowl_burnt_rices[1] = &transform;
		else if (transform.name == "BowlBurntRice2") bowl_burnt_rices[2] = &transform;
	}
	if (player == nullptr) throw std::runtime_error("GameObject not found.");
	if (bowls[0] == nullptr) throw std::runtime_error("GameObject not found.");
	if (bowls[1] == nullptr) throw std::runtime_error("GameObject not found.");
	if (bowls[2] == nullptr) throw std::runtime_error("GameObject not found.");
	if (bowl_uncooked_rices[0] == nullptr) throw std::runtime_error("GameObject not found.");
	if (bowl_uncooked_rices[1] == nullptr) throw std::runtime_error("GameObject not found.");
	if (bowl_uncooked_rices[2] == nullptr) throw std::runtime_error("GameObject not found.");
	if (bowl_cooked_rices[0] == nullptr) throw std::runtime_error("GameObject not found.");
	if (bowl_cooked_rices[1] == nullptr) throw std::runtime_error("GameObject not found.");
	if (bowl_cooked_rices[2] == nullptr) throw std::runtime_error("GameObject not found.");
	if (bowl_burnt_rices[0] == nullptr) throw std::runtime_error("GameObject not found.");
	if (bowl_burnt_rices[1] == nullptr) throw std::runtime_error("GameObject not found.");
	if (bowl_burnt_rices[2] == nullptr) throw std::runtime_error("GameObject not found.");

	//get pointer to camera for convenience:
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();

	for (Scene::Transform* bowl : bowls)
		bowl->position = off_screen;
	for (Scene::Transform* bowl : bowl_uncooked_rices)
		bowl->position = off_screen;
	for (Scene::Transform* bowl : bowl_cooked_rices)
		bowl->position = off_screen;
	for (Scene::Transform* bowl : bowl_burnt_rices)
		bowl->position = off_screen;

	update_camera();

	//start music loop playing:
	// (note: position will be over-ridden in update())
	background_loop = Sound::loop(*background_loop_sample, 0.3f, 0.0f);
}

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_KEYDOWN) {
		if (evt.key.keysym.sym == SDLK_ESCAPE) {
			SDL_SetRelativeMouseMode(SDL_FALSE);
			return true;
		} else if (evt.key.keysym.sym == SDLK_a) {
			left.downs += 1;
			left.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			right.downs += 1;
			right.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			up.downs += 1;
			up.pressed = true;
			return true;
		}
		else if (evt.key.keysym.sym == SDLK_s) {
			down.downs += 1;
			down.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_SPACE && !space.pressed) {
			space.downs += 1;
			space.pressed = true;
			return true;
		}
	} else if (evt.type == SDL_KEYUP) {
		if (evt.key.keysym.sym == SDLK_a) {
			left.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			right.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			up.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_SPACE && space.pressed) {
			space.pressed = false;
			return true;
		}
	}
	else if (evt.type == SDL_MOUSEBUTTONDOWN) {
		if (evt.button.button == SDL_BUTTON_RIGHT && SDL_GetRelativeMouseMode() == SDL_FALSE) {
			SDL_SetRelativeMouseMode(SDL_TRUE);
			looking = true;
			return true;
		}
	}
	else if (evt.type == SDL_MOUSEBUTTONUP) {
		if (evt.button.button == SDL_BUTTON_RIGHT && SDL_GetRelativeMouseMode() == SDL_TRUE) {
			SDL_SetRelativeMouseMode(SDL_FALSE);
			looking = false;
			return true;
		}
	}
	else if (evt.type == SDL_MOUSEMOTION) {
		if (SDL_GetRelativeMouseMode() == SDL_TRUE) {
			glm::vec2 motion = glm::vec2(
				evt.motion.xrel / float(window_size.y),
				-evt.motion.yrel / float(window_size.y)
			);

			yaw += 2.5f * -motion.x * camera->fovy;
			if (yaw < -M_PI) yaw += 2.0f * float(M_PI);
			if (yaw >= M_PI) yaw -= 2.0f * float(M_PI);

			float new_pitch = pitch + 1.5f * -motion.y * camera->fovy;
			if (new_pitch > pitch || !limit_pitch) {
				pitch = new_pitch;
				limit_pitch = false;
			}
			if (pitch < -M_PI / 2) pitch = float(-M_PI) / 2;
			if (pitch > M_PI / 2) pitch = float(M_PI) / 2;

			update_camera();

			return true;
		}
	}
	else if (evt.type == SDL_MOUSEWHEEL) {
		if (evt.wheel.y > 0 && camera_dist > 2.0f) {
			camera_dist -= 1.0f;
		}
		else if (evt.wheel.y < 0 && camera_dist < 10.0f) {
			camera_dist += 1.0f;
		}
		camera->transform->position.x = cos(yaw) * camera_dist + player->position.x + transformed_cam_offset.x;
		camera->transform->position.y = sin(yaw) * camera_dist + player->position.y + transformed_cam_offset.y;
		camera->transform->position.z = sin(pitch) * camera_dist + player->position.z + transformed_cam_offset.z;
		return true;
	}


	return false;
}

void PlayMode::update(float elapsed) {
	//combine inputs into a move:
	constexpr float PlayerSpeed = 10.0f;
	glm::vec2 move = glm::vec2(0.0f);
	if (left.pressed && !right.pressed) move.x =-1.0f;
	if (!left.pressed && right.pressed) move.x = 1.0f;
	if (down.pressed && !up.pressed) move.y =-1.0f;
	if (!down.pressed && up.pressed) move.y = 1.0f;

	//make it so that moving diagonally doesn't go faster:
	if (move != glm::vec2(0.0f)) move = glm::normalize(move) * PlayerSpeed * elapsed;

	//update player and camera movement
	{
		glm::mat4x3 frame = player->make_local_to_world();
		glm::vec3 forward = -glm::normalize(frame[0]);
		glm::vec3 right = glm::normalize(frame[1]);
		glm::vec3 up = glm::normalize(frame[2]);
		transformed_cam_offset = forward * camera_offset.x + right * camera_offset.y + up * camera_offset.z;
		transformed_hold_offset = forward * player_hold_offset.x + right * player_hold_offset.y + up * player_hold_offset.z;

		if (!looking) {
			player->position += move.x * right + move.y * forward;
		}
		else {
			frame = camera->transform->make_local_to_parent();
			right = frame[0];
			right.z = 0.0f;
			right = glm::normalize(right);
			forward = frame[2];
			forward.z = 0.0f;
			forward = glm::normalize(forward);
			player->position += move.x * right - move.y * forward;
		}
	}
	if (player->position.x < game_area.x_min)
		player->position.x = game_area.x_min;
	if (player->position.x > game_area.x_max)
		player->position.x = game_area.x_max;
	if (player->position.y < game_area.y_min)
		player->position.y = game_area.y_min;
	if (player->position.y > game_area.y_max)
		player->position.y = game_area.y_max;

	player_hold = player->position + transformed_hold_offset;

	update_camera();

	if (move != glm::vec2(0.0f)) {
		if (looking) target_player_yaw = yaw;
		if (target_player_yaw != player_yaw) {
			float diff = target_player_yaw - player_yaw;
			if (diff > M_PI) {
				diff -= 2.0f * float(M_PI);
			}
			else if (diff < -M_PI) {
				diff += 2.0f * float(M_PI);
			}
			if (abs(diff) < turn_rate) {
				player_yaw = target_player_yaw;
			}
			else if (diff > 0.0f) {
				player_yaw += turn_rate;
			}
			else {
				player_yaw -= turn_rate;
			}
		}
		player->rotation.w = cos(player_yaw / 2.0f);
		player->rotation.z = sin(player_yaw / 2.0f);
	}

	{ //update listener to camera position:
		glm::mat4x3 frame = player->make_local_to_world();
		glm::vec3 right = glm::normalize(frame[1]);
		glm::vec3 at = frame[3];
		Sound::listener.set_position_right(at, right, 1.0f / 60.0f);
	}
	
	//resolve interactive objects
	if (space.downs > 0) {
		int rice_spot = player_at_rice_cooker();
		if (held_item == ITEM_NONE && player_at_spot(bowl_stack) && bowls_out < 3) {
			int bowl_idx = get_available_bowl();
			if (bowl_idx != -1) {
				held_item = BOWL;
				held_item_obj = bowls[bowl_idx];
				bowls_out++;
			}
		}
		else if (held_item == BOWL && player_at_spot(rice_box)) {
			int bowl_idx = get_available_bowl_uncooked();
			if (bowl_idx != -1) {
				held_item = BOWL_UNCOOKED_RICE;
				held_item_obj->position = off_screen;
				held_item_obj = bowl_uncooked_rices[bowl_idx];
			}
		}
		else if (held_item == BOWL_UNCOOKED_RICE && rice_spot >= 0 && rice_states[rice_spot].state == STATE_NONE) {
			int bowl_idx = get_available_bowl();
			if (bowl_idx != -1) {
				held_item = BOWL;
				held_item_obj->position = off_screen;
				held_item_obj = bowls[bowl_idx];
				rice_states[rice_spot].state = UNREADY;
				rice_states[rice_spot].timer = 0.0f;
				boiling_water[rice_spot] = Sound::play_3D(*boiling_water_sample, 1.0f, rice_cookers[rice_spot], 0.75f);
			}
		}
		else if (held_item == BOWL && rice_spot >= 0 && rice_states[rice_spot].state == READY) {
			int bowl_idx = get_available_bowl_cooked();
			if (bowl_idx != -1) {
				held_item = BOWL_COOKED_RICE;
				held_item_obj->position = off_screen;
				held_item_obj = bowl_cooked_rices[bowl_idx];
				boiling_water[rice_spot]->stop();
				rice_states[rice_spot].state = STATE_NONE;
			}
		}
		else if (held_item == BOWL && rice_spot >= 0 && rice_states[rice_spot].state == BURNT) {
			int bowl_idx = get_available_bowl_burnt();
			if (bowl_idx != -1) {
				held_item = BOWL_BURNT_RICE;
				held_item_obj->position = off_screen;
				held_item_obj = bowl_burnt_rices[bowl_idx];
				boiling_water[rice_spot]->stop();
				rice_states[rice_spot].state = STATE_NONE;
			}
		}
		else if (held_item == BOWL_COOKED_RICE && player_at_spot(delivery_point)) {
			held_item = ITEM_NONE;
			held_item_obj->position = off_screen;
			held_item_obj = nullptr;
			complete_ticket(RICE);
			bowls_out--;
		}
		else if (player_at_spot(trash_can) && held_item != ITEM_NONE && held_item != BOWL) {
			int bowl_idx = get_available_bowl();
			if (bowl_idx != -1) {
				held_item_obj->position = off_screen;
				held_item = BOWL;
				held_item_obj = bowls[bowl_idx];
			}
		}
	}
	
	if (held_item_obj != nullptr)
		held_item_obj->position = player_hold;

	// update timers
	update_rice_cookers(elapsed);

	ticket_new_timer += elapsed;
	if (ticket_new_timer > ticket_new_duration) {
		spawn_new_ticket();
		ticket_new_timer = 0.0f;
	}

	update_tickets(elapsed);

	//reset button press counters:
	left.downs = 0;
	right.downs = 0;
	up.downs = 0;
	down.downs = 0;
	space.downs = 0;
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	//update camera aspect ratio for drawable:
	camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	//set up light type and position for lit_color_texture_program:
	// TODO: consider using the Light(s) in the scene to do this
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
	glUseProgram(0);

	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	scene.draw(*camera);

	{ //use DrawLines to overlay some text:
		glDisable(GL_DEPTH_TEST);
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		));

		constexpr float H = 0.15f;
		if (player_at_spot(delivery_point)) {
			lines.draw_text("Press spacebar to deliver food",
				glm::vec3(-0.5f * aspect, -1.0 + 0.1f * H, 0.0),
				glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
				glm::u8vec4(0xff, 0xff, 0xff, 0x00));
		}
		else if (player_at_spot(bowl_stack)) {
			lines.draw_text("Press spacebar to pick up bowl",
				glm::vec3(-0.5f * aspect, -1.0 + 0.1f * H, 0.0),
				glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
				glm::u8vec4(0xff, 0xff, 0xff, 0x00));
		}
		else if (player_at_spot(rice_box)) {
			lines.draw_text("Press spacebar to pick up rice",
				glm::vec3(-0.5f * aspect, -1.0 + 0.1f * H, 0.0),
				glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
				glm::u8vec4(0xff, 0xff, 0xff, 0x00));
		}
		else if (player_at_rice_cooker() != -1) {
			lines.draw_text("Press spacebar to use rice cooker",
				glm::vec3(-0.5f * aspect, -1.0 + 0.1f * H, 0.0),
				glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
				glm::u8vec4(0xff, 0xff, 0xff, 0x00));
		}
		else if (player_at_spot(trash_can)) {
			lines.draw_text("Press spacebar to throw out food",
				glm::vec3(-0.5f * aspect, -1.0 + 0.1f * H, 0.0),
				glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
				glm::u8vec4(0xff, 0xff, 0xff, 0x00));
		}

		constexpr float S = 0.08f;
		for (int i = 0; i < num_tickets; i++) {
			Ticket ticket = tickets[i];
			if (ticket.type == RICE) {
				if (int(ticket.duration - ticket.timer) < 5) {
					lines.draw_text("Order: Plain rice. Time: " + std::to_string(int(ticket.duration - ticket.timer)),
						glm::vec3(-1.0f * aspect + 0.1f * S, 1.0 - 1.1f * S * (i + 1), 0.0),
						glm::vec3(S, 0.0f, 0.0f), glm::vec3(0.0f, S, 0.0f),
						glm::u8vec4(0xff, 0x00, 0x00, 0x00));
				}
				else {
					lines.draw_text("Order: Plain rice. Time: " + std::to_string(int(ticket.duration - ticket.timer)),
						glm::vec3(-1.0f * aspect + 0.1f * S, 1.0 - 1.1f * S * (i + 1), 0.0),
						glm::vec3(S, 0.0f, 0.0f), glm::vec3(0.0f, S, 0.0f),
						glm::u8vec4(0xff, 0xff, 0xff, 0x00));
				}
			}
		}

		lines.draw_text("Score: " + std::to_string(int(score)),
			glm::vec3(0.65 * aspect, 1.0 - 1.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
	}
}
