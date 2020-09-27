#include "Mode.hpp"

#include "Scene.hpp"
#include "Sound.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>
#include <random>

struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//helper functions
	void update_camera();
	bool player_at_spot(glm::vec3 spot);
	int player_at_rice_cooker();
	void reset_game();
	void spawn_new_ticket();
	int get_available_bowl();
	int get_available_bowl_uncooked();
	int get_available_bowl_cooked();
	int get_available_bowl_burnt();
	void update_rice_cookers(float elapsed);

	enum Ticket_type { TICKET_NONE, RICE, CURRY };
	void complete_ticket(Ticket_type type);
	void update_tickets(float elapsed);

	//----- game state -----

	//input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} left, right, down, up, space;

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;

	Scene::Transform* player = nullptr;
	Scene::Transform* bowls[3] = { nullptr, nullptr, nullptr };
	Scene::Transform* bowl_uncooked_rices[3] = { nullptr, nullptr, nullptr };
	Scene::Transform* bowl_cooked_rices[3] = { nullptr, nullptr, nullptr };
	Scene::Transform* bowl_burnt_rices[3] = { nullptr, nullptr, nullptr };

	float camera_dist = 10.0f;
	bool looking = false;

	glm::vec3 camera_target = glm::vec3(0.0f, 0.0f, 1.0f);
	glm::vec3 camera_up = glm::vec3(0.0f, 0.0f, 1.0f);
	glm::vec3 camera_dir;
	glm::vec3 camera_offset = glm::vec3(0.0f, 0.0f, 1.0f);
	glm::vec3 transformed_cam_offset = camera_offset;

	float floor = 0.1f;
	bool limit_pitch = false;

	float pitch = 0.0f, yaw = 0.0f;
	float player_yaw = 0.0f;
	float target_player_yaw = 0.0f;
	float turn_rate = 0.15f;

	float box_radius = 0.25f;
	float box_height = 0.15f;

	struct Bounds {
		float x_min = -12.0f;
		float x_max = 12.0f;
		float y_min = -12.0f;
		float y_max = 12.0f;
		float x_size = 24.0f;
		float y_size = 24.0f;
	} game_area;

	glm::vec3 player_hold_offset = glm::vec3(0.55f, 0.0f, 0.85f);
	glm::vec3 transformed_hold_offset;
	glm::vec3 player_hold;
	glm::vec3 off_screen = glm::vec3(0.0f, 0.0f, -10.0f);

	glm::vec3 delivery_point = glm::vec3(-12.0f, 0.0f, 0.75f);
	glm::vec3 bowl_stack = glm::vec3(-8.0f, 2.0f, 0.75f);
	glm::vec3 rice_box = glm::vec3(-6.0f, 2.0f, 0.75f);
	glm::vec3 rice_cookers[3] = { glm::vec3(0.0f, 2.0f, 0.5f), glm::vec3(0.0f, 0.0f, 0.5f), glm::vec3(0.0f, -2.0f, 0.5f) };
	glm::vec3 trash_can = glm::vec3(-8.0f, -2.0f, 0.75f);

	float max_player_speed = 7.5f;
	float min_player_speed = 2.5f;

	enum Item { ITEM_NONE, BOWL, BOWL_UNCOOKED_RICE, BOWL_COOKED_RICE, BOWL_BURNT_RICE };
	Item held_item = ITEM_NONE;
	Scene::Transform* held_item_obj = nullptr;

	int bowls_out = 0;

	std::mt19937 mt;

	enum State { STATE_NONE, UNREADY, READY, BURNT };
	struct RiceState {
		State state = STATE_NONE;
		float timer = 0.0f;
	};
	RiceState rice_states[3];
	float rice_min_duration = 10.0f;
	float rice_max_duration = 20.0f;

	struct Ticket {
		Ticket_type type = TICKET_NONE;
		float timer = 0.0f;
		float duration = 0.0f;
	};
	typedef struct Ticket Ticket;
	static const int num_tickets = 4;
	Ticket tickets[num_tickets];
	int ticket_spawn = 0;
	float ticket_new_timer = 0.0f;
	float ticket_new_min = 5.0f;
	float ticket_new_range = 5.0f;
	float ticket_new_duration = ticket_new_min + (mt() / float(mt.max())) * ticket_new_range;
	float ticket_duration_min = 16.0f;
	float ticket_duration_range = 6.0f;

	float score = 0.0f;

	//music coming from the tip of the leg (as a demonstration):
	std::shared_ptr< Sound::PlayingSample > background_loop;
	std::shared_ptr< Sound::PlayingSample > bell_ding;
	std::shared_ptr< Sound::PlayingSample > boiling_water[3];
	
	//camera:
	Scene::Camera *camera = nullptr;

};
