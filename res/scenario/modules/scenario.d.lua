--- @meta
scenario = {}

--- @class ivec2
--- @field x number
--- @field y number

scenario.SQUAD_ID_NULL = -1
scenario.CHAT_COLOR_GOLD = 1
scenario.CHAT_COLOR_BLUE = 1
scenario.bot_config_flag = {}
scenario.bot_config_flag.SHOULD_HARASS = 4
scenario.bot_config_flag.SHOULD_PRODUCE = 64
scenario.bot_config_flag.SHOULD_ATTACK_FIRST = 1
scenario.bot_config_flag.SHOULD_SCOUT = 16
scenario.bot_config_flag.SHOULD_SURRENDER = 32
scenario.bot_config_flag.SHOULD_ATTACK = 2
scenario.bot_config_flag.SHOULD_CANCEL_BUILDINGS = 128
scenario.bot_config_flag.SHOULD_RETREAT = 8

scenario.CAMERA_MODE_PAN = 2
scenario.entity_mode = {}
scenario.entity_mode.UNIT_MOVE_FINISHED = 3
scenario.entity_mode.UNIT_SOLDIER_RANGED_ATTACK_WINDUP = 8
scenario.entity_mode.MINE_PRIME = 20
scenario.entity_mode.UNIT_BALLOON_DEATH = 15
scenario.entity_mode.UNIT_BUILD = 4
scenario.entity_mode.MINE_ARM = 19
scenario.entity_mode.UNIT_MOVE = 2
scenario.entity_mode.UNIT_BUILD_ASSIST = 5
scenario.entity_mode.UNIT_IN_MINE = 10
scenario.entity_mode.UNIT_IDLE = 0
scenario.entity_mode.UNIT_DEATH_FADE = 13
scenario.entity_mode.UNIT_DEATH = 12
scenario.entity_mode.UNIT_BALLOON_DEATH_START = 14
scenario.entity_mode.GOLDMINE = 21
scenario.entity_mode.BUILDING_DESTROYED = 18
scenario.entity_mode.UNIT_REPAIR = 6
scenario.entity_mode.BUILDING_FINISHED = 17
scenario.entity_mode.UNIT_PYRO_THROW = 11
scenario.entity_mode.BUILDING_IN_PROGRESS = 16
scenario.entity_mode.GOLDMINE_COLLAPSED = 22
scenario.entity_mode.UNIT_SOLDIER_CHARGE = 9
scenario.entity_mode.UNIT_ATTACK_WINDUP = 7
scenario.entity_mode.UNIT_BLOCKED = 1

scenario.entity_type = {}
scenario.entity_type.BARRACKS = 19
scenario.entity_type.SHERIFFS = 20
scenario.entity_type.CANNON = 9
scenario.entity_type.WAGON = 4
scenario.entity_type.LANDMINE = 21
scenario.entity_type.BALLOON = 11
scenario.entity_type.WORKSHOP = 16
scenario.entity_type.BANDIT = 3
scenario.entity_type.SALOON = 14
scenario.entity_type.PYRO = 7
scenario.entity_type.SOLDIER = 8
scenario.entity_type.BUNKER = 15
scenario.entity_type.GOLDMINE = 0
scenario.entity_type.SAPPER = 6
scenario.entity_type.COOP = 18
scenario.entity_type.HOUSE = 13
scenario.entity_type.JOCKEY = 5
scenario.entity_type.COWBOY = 2
scenario.entity_type.HALL = 12
scenario.entity_type.DETECTIVE = 10
scenario.entity_type.SMITH = 17
scenario.entity_type.MINER = 1

scenario.CAMERA_MODE_FREE = 0
scenario.target_type = {}
scenario.target_type.NONE = 0
scenario.target_type.REPAIR = 5
scenario.target_type.CELL = 1
scenario.target_type.PATROL = 10
scenario.target_type.UNLOAD = 6
scenario.target_type.BUILD = 8
scenario.target_type.ENTITY = 2
scenario.target_type.BUILD_ASSIST = 9
scenario.target_type.ATTACK_ENTITY = 4
scenario.target_type.MOLOTOV = 7
scenario.target_type.ATTACK_CELL = 3

scenario.CHAT_COLOR_WHITE = 0
scenario.global_objective_counter_type = {}
scenario.global_objective_counter_type.OFF = 0
scenario.global_objective_counter_type.GOLD = 1

scenario.bot_squad_type = {}
scenario.bot_squad_type.DEFEND = 1
scenario.bot_squad_type.LANDMINES = 3
scenario.bot_squad_type.ATTACK = 0
scenario.bot_squad_type.RESERVES = 2
scenario.bot_squad_type.PATROL = 5
scenario.bot_squad_type.RETURN = 4
scenario.bot_squad_type.HOLD_POSITION = 6

scenario.match_input_type = {}
scenario.match_input_type.NONE = 0
scenario.match_input_type.MOVE_ENTITY = 2
scenario.match_input_type.MOVE_ATTACK_CELL = 3
scenario.match_input_type.BUILD = 10
scenario.match_input_type.MOVE_ATTACK_ENTITY = 4
scenario.match_input_type.BUILDING_DEQUEUE = 13
scenario.match_input_type.PATROL = 19
scenario.match_input_type.BUILDING_ENQUEUE = 12
scenario.match_input_type.RALLY = 14
scenario.match_input_type.CAMO = 17
scenario.match_input_type.MOVE_MOLOTOV = 7
scenario.match_input_type.BUILD_CANCEL = 11
scenario.match_input_type.DEFEND = 9
scenario.match_input_type.MOVE_REPAIR = 5
scenario.match_input_type.MOVE_UNLOAD = 6
scenario.match_input_type.SINGLE_UNLOAD = 15
scenario.match_input_type.DECAMO = 18
scenario.match_input_type.UNLOAD = 16
scenario.match_input_type.MOVE_CELL = 1
scenario.match_input_type.STOP = 8

scenario.sound = {}
scenario.sound.BUILDING_DESTROY = 11
scenario.sound.CAMO_OFF = 29
scenario.sound.EXPLOSION = 4
scenario.sound.GUN = 3
scenario.sound.THROW = 16
scenario.sound.GARRISON_IN = 18
scenario.sound.BALLOON_DEATH = 30
scenario.sound.PICKAXE = 5
scenario.sound.GARRISON_OUT = 19
scenario.sound.UI_CLICK = 0
scenario.sound.BUNKER_DESTROY = 12
scenario.sound.ALERT_BUILDING = 21
scenario.sound.DEATH_CHICKEN = 9
scenario.sound.ALERT_RESEARCH = 22
scenario.sound.OBJECTIVE_COMPLETE = 32
scenario.sound.SWORD = 8
scenario.sound.MINE_PRIME = 15
scenario.sound.CANNON = 10
scenario.sound.ALERT_BELL = 20
scenario.sound.MUSKET = 2
scenario.sound.DEATH = 1
scenario.sound.MATCH_START = 33
scenario.sound.RICOCHET = 31
scenario.sound.CAMO_ON = 28
scenario.sound.GOLD_MINE_COLLAPSE = 24
scenario.sound.MOLOTOV_IMPACT = 25
scenario.sound.MINE_INSERT = 14
scenario.sound.FIRE_BURN = 26
scenario.sound.ALERT_UNIT = 23
scenario.sound.BUILDING_PLACE = 7
scenario.sound.PISTOL_SILENCED = 27
scenario.sound.MINE_DESTROY = 13
scenario.sound.FLAG_THUMP = 17
scenario.sound.HAMMER = 6

scenario.CAMERA_MODE_MINIMAP_DRAG = 1
scenario.PLAYER_ID = 0
scenario.CAMERA_MODE_HELD = 3

--- Send a debug log. If debug logging is disabled, this function does nothing.
--- @param ... any Values to print
function scenario.log(...) end

--- Plays a sound effect.
--- @param sound number
function scenario.play_sound(sound) end

--- Returns the time in seconds since the scenario started.
--- @return number
function scenario.get_time() end

--- End the match in victory.
function scenario.set_match_over_victory() end

--- End the match in defeat.
function scenario.set_match_over_defeat() end

--- Checks if the player has been defeated.
--- @param player_id number
--- @return boolean 
function scenario.player_is_defeated(player_id) end

--- Returns the specified player's gold count
--- @param player_id number
--- @return number
function scenario.player_get_gold(player_id) end

--- Returns the total number of gold mined by the specified player this match
--- @param player_id number
--- @return number
function scenario.player_get_gold_mined_total(player_id) end

--- Returns the number of entities controlled by the player of a given type.
--- @param player_id number
--- @param entity_type number
--- @return number
function scenario.player_get_entity_count(player_id, entity_type) end

--- Returns the number of bunkers controlled by the player that have 4 cowboys in them.
--- @param player_id number
--- @return number
function scenario.player_get_full_bunker_count(player_id) end

--- Returns true if the specified player has an entity near the cell within the given distance
--- @param player_id number
--- @param cell ivec2
--- @param distance number
--- @return boolean
function scenario.player_has_entity_near_cell(player_id, cell, distance) end

--- Sends a chat message.
--- @param color number
--- @param prefix string
--- @param message string
function scenario.chat(color, prefix, message) end

--- Sends a hint message.
--- @param message string
function scenario.hint(message) end

--- Reveals fog at the specified cell.
--- @param params { cell: ivec2, cell_size: number, sight: number, duration: number }
function scenario.fog_reveal(params) end

--- Returns the cell the camera is currently centered on
--- @return ivec2
function scenario.get_camera_centered_cell() end

--- Gradually pans the camera to center on the specified cell. 
--- @param cell ivec2 The cell to pan the camera to
--- @param duration number The duration in seconds of the camera pan
function scenario.begin_camera_pan(cell, duration) end

--- Removes camera movement from the player and holds the camera in place
function scenario.hold_camera() end

--- Returns camera movement to the player
function scenario.release_camera() end

--- Returns the current camera mode.
--- @return number
function scenario.get_camera_mode() end

--- Adds an objective to the objectives list.
--- 
--- If entity_type is provided, then counter_target is also required and 
--- the objective counter will be based on the player's entities of that type.
--- 
--- If counter_target is provided but entity_type is not, then the objective counter
--- will be a variable counter that must be manually updated.
--- 
--- Returns the index of the created objective.
--- @param params { description: string, entity_type: number|nil, counter_target: number|nil }
function scenario.add_objective(params) end

--- Marks the specified objective as complete.
--- @param objective_index number
function scenario.complete_objective(objective_index) end

--- Returns true if the specified objective is complete.
--- @param objective_index number
--- @return boolean
function scenario.is_objective_complete(objective_index) end

--- Returns true if all objectives are complete.
--- @return boolean
function scenario.are_objectives_complete() end

--- Clears the objectives list.
function scenario.clear_objectives() end

--- Sets the global objective counter
--- @param value number
function scenario.set_global_objective_counter(value) end

--- Returns true if the specified entity is visible to the player.
--- @param entity_id number
--- @return boolean
function scenario.entity_is_visible_to_player(entity_id) end

--- Highlights the specified entity.
--- @param entity_id number
function scenario.highlight_entity(entity_id) end

--- Returns a table of information about the specified entity, or nil if the entity does not exist
--- @param entity_id number
--- @return table
function scenario.entity_get_info(entity_id) end

--- Returns the gold cost of the specified entity type
--- @param entity_type number
--- @return number
function scenario.entity_get_gold_cost(entity_type) end

--- If no cell is found, returns nil, but this should be rare.
--- @param entity_type number
--- @param spawn_cell ivec2
--- @return ivec2|nil
function scenario.entity_find_spawn_cell(entity_type, spawn_cell) end

--- Creates a new entity. Returns the ID of the newly created entity
--- @param entity_type number
--- @param cell ivec2
--- @param player_id number
--- @return number
function scenario.entity_create(entity_type, cell, player_id) end

--- Spawns an enemy squad. The entities table should be an array of entity types.
--- Returns the squad ID of the created squad, or SQUAD_ID_NULL if no squad was created.
--- @param params { player_id: number, type: number, target_cell: ivec2, entities: table }
--- @return number 
function scenario.bot_add_squad(params) end

--- Checks if the squad exists.
--- @param player_id number
--- @param squad_id number
--- @return boolean 
function scenario.bot_squad_exists(player_id, squad_id) end

--- Sets a bot config flag to the specified value
--- @param player_id number
--- @param flag number
--- @param value boolean
function scenario.bot_set_config_flag(player_id, flag, value) end

--- Tells the specified bot to reserve the specified entity
--- @param player_id number
--- @param entity_id number
function scenario.bot_reserve_entity(player_id, entity_id) end

--- Tells the specified bot o release the specified entity
--- @param player_id number
--- @param entity_id number
function scenario.bot_release_entity(player_id, entity_id) end

--- Queues a match input
--- 
--- MOVE { target_cell: ivec2|nil, target_id: number|nil }
--- BUILD { building_type: number, building_cell: ivec2 }
--- @param params { player_id: number, type: number, entity_id: number|nil, entity_ids: table|nil, ... }
function scenario.queue_match_input(params) end

