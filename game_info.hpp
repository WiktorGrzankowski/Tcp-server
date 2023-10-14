struct GameInfo {
    bool has_stared;
    std::string server_name;
    uint8_t players_count;
    coordinate_t size_x;
    coordinate_t size_y;
    uint16_t game_length;
    uint16_t explosion_radius;
    uint16_t bomb_timer;
    PlayersMap players;
    PositionsMap player_positions;
    std::set<Position> blocks;
    std::map<uint32_t, Bomb> bombs;
    std::vector<Position> explosions;
    ScoresMap scores;
    uint16_t turn;

    void update_with_hello_info(HelloMessage &message) {
        has_stared = false;
        server_name = message.server_name;
        players_count = message.players_count;
        size_x = message.size_x;
        size_y = message.size_y;
        game_length = message.game_length;
        explosion_radius = message.explosion_radius;
        bomb_timer = message.bomb_timer;
    }

    void update_with_accepted_player_info(AcceptedPlayerMessage &message) {
        players[message.id] = message.player;
    }

    void update_with_game_started_info(GameStartedMessage &message) {
        players = message.players;
        turn = 0;
    }

    void update_with_game_ended_info(GameEndedMessage &message) {
        scores = message.scores;
    }

private:

    void update_with_single_event(Event::BlockPlaced &event) {
        blocks.insert(event.position);
    }

    void update_with_single_event(Event::PlayerMoved &event) {
        player_positions[event.id] = event.position;
    }

    void update_with_single_event(Event::BombPlaced &event) {
        bombs.insert({event.id, Bomb(event.position, bomb_timer)});
    }

    void update_with_single_event(Event::BombExploded &event) {
        bombs.erase(event.id);
        for (auto &destroyed_block_position : event.blocks_destroyed)
            blocks.erase(destroyed_block_position);

        // teraz robots destroyed, nie jestem pewien co tu ma sie wydarzyc
        // na pewno trzeba doliczyc, ze zgineli, ale poza tym to nie wiem
        // czy mamy powiedziec gdzie sie spawnuja, czy nie?
        for (auto &destroyed_robot_id : event.robots_destroyed) {
            scores[destroyed_robot_id]++;
        }
    }

public:
    // todo - w studia/sk/spr rozpisalem sobie jak to ma po sobie dziedziczyc i jest maśniutko
    // todo - poprawic to zeby sensownie dzialalo, najlepiej bez switcha zadnego. chyba nawet nie da sie switcha
    void update_with_turn_info(TurnMessage &message) {
        turn = message.turn;
        for (auto &event: message.events) {
//            update_with_single_event(event);
            // ugh cos nie tak jest z syntaxem, trzeba moze zrobic Event jako abstrakcyjna
            // i dodac te update do structów, żeby to po sobie dziedziczylo
//            update_with_single_event(event);
            // wszedzie zmniejszyc o 1 timer w bombie
        }
    }
};



GameInfo global_game_info;
