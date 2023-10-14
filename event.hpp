using score_t = uint32_t;
using player_id_t = uint8_t;
using coordinate_t = uint16_t;
using bomb_id_t = uint32_t;
using score_t = uint32_t;

struct Position {
    coordinate_t x;
    coordinate_t y;

    bool operator<(const Position &position) const {
        return this->x < position.x || (this->x == position.x && this->y < position.y);
    }
};

struct Bomb {
    Position position;
    uint16_t timer;

    Bomb(const Position &position, uint16_t timer) : position(position), timer(timer) {};
};
// TODO - zrobic tak jak pisalem w vs code, bedzie dziedziczenie funkcji
namespace Event {
    struct EventS {

    };

    struct BombPlaced : EventS {
        bomb_id_t id;
        Position position;

        BombPlaced(bomb_id_t i, Position position) : id(i), position(position) {};
    };

    struct BombExploded : EventS {
        bomb_id_t id;
        std::vector<player_id_t> robots_destroyed;
        std::vector<Position> blocks_destroyed;

        BombExploded(bomb_id_t id, const std::vector<player_id_t> &robotsDestroyed,
                     const std::vector<Position> &blocksDestroyed) : id(id), robots_destroyed(robotsDestroyed),
                                                                     blocks_destroyed(blocksDestroyed) {};
    };

    struct PlayerMoved : EventS {
        player_id_t id;
        Position position;

        PlayerMoved(player_id_t id, const Position &position) : id(id), position(position) {};
    };

    struct BlockPlaced : EventS {
        Position position;
        BlockPlaced(const Position &position) : position(position) {};
    };

}
