--- @meta
scenario = {}

--- @class ivec2
--- @field x number
--- @field y number

--- Script constants
scenario.CHAT_COLOR_WHITE = 0
scenario.CHAT_COLOR_GOLD = 1
scenario.CHAT_COLOR_BLUE = 1
scenario.SQUAD_ID_NULL = -1
scenario.CAMERA_MODE_FREE = 0
scenario.CAMERA_MODE_MINIMAP_DRAG = 1
scenario.CAMERA_MODE_PAN = 2
scenario.CAMERA_MODE_HELD = 3

--- Scenario constants
scenario.constants = {}

--- Entity constants
scenario.entity_type = {}
scenario.entity_type.GOLDMINE = 0
scenario.entity_type.MINER = 1
scenario.entity_type.COWBOY = 2
scenario.entity_type.BANDIT = 3
scenario.entity_type.WAGON = 4
scenario.entity_type.JOCKEY = 5
scenario.entity_type.SAPPER = 6
scenario.entity_type.PYRO = 7
scenario.entity_type.SOLDIER = 8
scenario.entity_type.CANNON = 9
scenario.entity_type.DETECTIVE = 10
scenario.entity_type.BALLOON = 11
scenario.entity_type.HALL = 12
scenario.entity_type.HOUSE = 13
scenario.entity_type.SALOON = 14
scenario.entity_type.BUNKER = 15
scenario.entity_type.WORKSHOP = 16
scenario.entity_type.SMITH = 17
scenario.entity_type.COOP = 18
scenario.entity_type.BARRACKS = 19
scenario.entity_type.SHERIFFS = 20
scenario.entity_type.LANDMINE = 21

--- Sound constants
scenario.sound = {}
scenario.sound.UI_CLICK = 0
scenario.sound.DEATH = 1
scenario.sound.MUSKET = 2
scenario.sound.GUN = 3
scenario.sound.EXPLOSION = 4
scenario.sound.PICKAXE = 5
scenario.sound.HAMMER = 6
scenario.sound.BUILDING_PLACE = 7
scenario.sound.SWORD = 8
scenario.sound.DEATH_CHICKEN = 9
scenario.sound.CANNON = 10
scenario.sound.BUILDING_DESTROY = 11
scenario.sound.BUNKER_DESTROY = 12
scenario.sound.MINE_DESTROY = 13
scenario.sound.MINE_INSERT = 14
scenario.sound.MINE_PRIME = 15
scenario.sound.THROW = 16
scenario.sound.FLAG_THUMP = 17
scenario.sound.GARRISON_IN = 18
scenario.sound.GARRISON_OUT = 19
scenario.sound.ALERT_BELL = 20
scenario.sound.ALERT_BUILDING = 21
scenario.sound.ALERT_RESEARCH = 22
scenario.sound.ALERT_UNIT = 23
scenario.sound.GOLD_MINE_COLLAPSE = 24
scenario.sound.MOLOTOV_IMPACT = 25
scenario.sound.FIRE_BURN = 26
scenario.sound.PISTOL_SILENCED = 27
scenario.sound.CAMO_ON = 28
scenario.sound.CAMO_OFF = 29
scenario.sound.BALLOON_DEATH = 30
scenario.sound.RICOCHET = 31
scenario.sound.OBJECTIVE_COMPLETE = 32
scenario.sound.MATCH_START = 33

--- Bot config constants
scenario.bot_config_flag = {}
scenario.bot_config_flag.SHOULD_ATTACK_FIRST = 1
scenario.bot_config_flag.SHOULD_ATTACK = 2
scenario.bot_config_flag.SHOULD_HARASS = 4
scenario.bot_config_flag.SHOULD_RETREAT = 8
scenario.bot_config_flag.SHOULD_SCOUT = 16
scenario.bot_config_flag.SHOULD_SURRENDER = 32
scenario.bot_config_flag.SHOULD_PAUSE = 64

--- Send a debug log. If debug logging is disabled, this function does nothing.
--- @param ... any Values to print
function scenario.log(...) end

--- End the match in victory.
function scenario.set_match_over_victory() end

--- End the match in defeat.
function scenario.set_match_over_defeat() end

--- Sends a chat message.
--- @param color number
--- @param prefix string
--- @param message string
function scenario.chat(color, prefix, message) end

--- Sends a hint message.
--- @param message string
function scenario.hint(message) end

--- Plays a sound effect.
--- @param sound number
function scenario.play_sound(sound) end

--- Returns the time in seconds since the scenario started.
--- @return number
function scenario.get_time() end

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

--- Returns true if the specified entity is visible to the player.
--- @param entity_id number
--- @return boolean
function scenario.entity_is_visible_to_player(entity_id) end

--- Highlights the specified entity.
--- @param entity_id number
function scenario.highlight_entity(entity_id) end

--- Returns the number of entities controlled by the player of a given type.
--- @param player_id number
--- @param entity_type number
--- @return number
function scenario.get_player_entity_count(player_id, entity_type) end

--- Returns the number of bunkers controlled by the player that have 4 cowboys in them.
--- @param player_id number
--- @return number
function scenario.get_player_full_bunker_count(player_id) end

--- Returns true if the specified player has an entity near the cell within the given distance
--- @param player_id number
--- @param cell ivec2
--- @param distance number
--- @return boolean
function scenario.player_has_entity_near_cell(player_id, cell, distance) end

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

--- Spawns an enemy squad. The entities table should be an array of entity types.
--- Returns the squad ID of the created squad, or SQUAD_ID_NULL if no squad was created.
--- @param params { player_id: number, target_cell: ivec2, spawn_cell: ivec2, entities: table }
--- @return number 
function scenario.spawn_enemy_squad(params) end

--- Checks if the squad exists.
--- @param player_id number
--- @param squad_id number
--- @return boolean 
function scenario.does_squad_exist(player_id, squad_id) end

--- Checks if the player has been defeated.
--- @param player_id number
--- @return boolean 
function scenario.is_player_defeated(player_id) end

--- Queues a build input
--- @param params { building_type: number, building_cell: ivec2, builder_id: number }
function scenario.match_input_build(params) end

--- Sets a bot config flag to the specified value
--- @param player_id number
--- @param flag number
--- @param value boolean
function scenario.set_bot_flag(player_id, flag, value) end

