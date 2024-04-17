-- Vacuum Cleaner Firmware
-- Modes:
-- 1 = NO MONEY
-- 2 = SOME_MONEY_NOT_STARTED
-- 3 = STARTED_AND_WORKING
-- 4 = THANKS

local programs = {
    stop = 0,
    run = 1
}

local modes = {
    no_money = 1,
    waiting_money = 2,
    working = 3,
    thanks = 4
}

local version = "0.1"
local price_per_minute = 10.0
local thanks_mode_duration_sec = 20
local balance = 0.0
local mode = modes.no_money
local current_program = programs.stop
local ms_per_loop = 100
local iterations_per_second = 1000 / ms_per_loop

local function update_balance(screen)
    balance = balance + hardware:GetCoins() + hardware:GetBanknotes()

    if screen then
        screen:Set("balance.value", math.ceil(balance))
        screen:Display()
    end
end

local function set_current_state(balance, program)
    hardware:SetCurrentState(math.floor(balance), program)
end

local function smart_delay(ms)
    return hardware:SmartDelay(ms)
end

local function turn_light(rel_num, animation_code)
    hardware:TurnLight(rel_num, animation_code)
end

local function get_price(key)
    return registry:GetPrice(key)
end

local function get_key()
    return hardware:GetKey()
end

local function run_program(program_num)
    current_program = program_num
    hardware:TurnProgram(program_num)
end

local function is_mode_selection_key(key)
    return key > 0 and key < 7
end

local function deduct_balance(screen)
    balance = balance - price_per_minute * ms_per_loop / 60000
    if balance < 0 then balance = 0 end

    set_current_state(balance, current_program)

    if screen then
        screen:Set("balance.value", math.ceil(balance))
        screen:Display()
    end
end

local function welcome_mode()
    price_per_minute = get_price(programs.run)

    welcome:Display()
    turn_light(1, animation.stop)
    run_program(programs.stop)

    update_balance()
    return balance > 0.99 and modes.waiting_money or modes.no_money
end

local function waiting_money_mode()
    turn_light(1, animation.one_button)
    run_program(programs.stop)
    update_balance(moremoney)

    return get_key() == modes.no_money and modes.working or modes.waiting_money
end

local function working_mode()
    turn_light(1, animation.one_button)
    run_program(programs.run)

    update_balance(working)
    if balance < 0.01 then return modes.thanks end

    deduct_balance(working)
    return modes.working
end

local thanks_waiting_loops = 0

local function reset_thanks_mode()
    thanks_waiting_loops = 0
end

local function thanks_mode_init()
    thanks_waiting_loops = thanks_mode_duration_sec * iterations_per_second
    thanks:Display()
    turn_light(1, animation.stop)
    run_program(programs.stop)
    balance = 0
end

local function thanks_mode()
    
    if thanks_waiting_loops == 0 then
        thanks_mode_init()
    end

    update_balance()

    if balance > 0.99 then
        reset_thanks_mode()
        return modes.waiting_money
    end

    local key = get_key()
    if is_mode_selection_key(key) then
        reset_thanks_mode()
        return modes.no_money
    end

    thanks_waiting_loops = thanks_waiting_loops - 1
    if thanks_waiting_loops <= 0 then
        reset_thanks_mode()
        return modes.no_money
    end

    return modes.thanks
end


local function run_mode(new_mode)
    if new_mode == modes.no_money then return welcome_mode() end
    if new_mode == modes.waiting_money then return waiting_money_mode() end
    if new_mode == modes.working then return working_mode() end
    if new_mode == modes.thanks then return thanks_mode() end
end

function setup()
    printMessage("Vacuum Cleaner Sample v." .. version)
    return 0
end

function loop()
    mode = run_mode(mode)
    ms_per_loop = smart_delay(ms_per_loop)
    iterations_per_second = 1000 / ms_per_loop

    return 0
end
