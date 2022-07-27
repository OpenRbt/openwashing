-- Vacuum Firmware

-- setup is running at the start just once
setup = function()
    -- global variables
    balance = 0.0

    min_electron_balance = 10
    max_electron_balance = 900
    electron_balance = 0
    free_pause_seconds = 0
    volume = 0
    
    balance_seconds = 0
    cash_balance = 0.0
    electronical_balance = 0.0
    post_position = 1

    keyboard_pressed = false 

    -- constants
    welcome_mode_seconds = 3
    thanks_mode_seconds = 5
    wait_mode_seconds = 40
    
    is_transaction_started = false
    is_waiting_receipt = false

    -- buttons
    button_cash = 100
    button_cashless = 101
    button_begin = 102
    button_cancel = 103

    price_p = {}
    
    price_p[0] = 0
    price_p[1] = 0

    init_prices()
    
    mode_welcome = 10
    mode_choose = 20
    mode_fundraising = 30
    mode_keyboard = 40
    mode_wait = 50
    mode_start_filling = 60
    mode_filling = 70
    mode_thanks = 80
    mode_apology = 90

    real_ms_per_loop = 100
    
    current_mode = mode_welcome

    version = "1.0.0"

    printMessage("dia generic wash firmware v." .. version)
    smart_delay(1)
    return 0
end

loop = function()
    printMessage("current_mode: " .. current_mode)
    current_mode = run_mode(current_mode)
    real_ms_per_loop = smart_delay(100)
    return 0
end

init_prices = function()
    price_p[1] = get_price(1)
end

-- Mode

run_mode = function(new_mode)   
    if new_mode == mode_welcome then return welcome_mode() end
    if new_mode == mode_choose then return choose_mode() end
    if new_mode == mode_fundraising then return fundraising_mode() end
    if new_mode == mode_keyboard then return keyboard_mode() end
    if new_mode == mode_wait then return wait_mode() end
    if new_mode == mode_start_filling then return start_filling_mode() end
    if new_mode == mode_filling then return filling_mode() end
    if new_mode == mode_thanks then return thanks_mode() end
    if new_mode == mode_apology then return apology_mode() end
end

welcome_mode = function()
    show_welcome()
    run_stop()
    turn_light(0, animation.idle)
    smart_delay(1000 * welcome_mode_seconds)
    forget_pressed_key()
    return mode_choose
end

choose_mode = function()
    show_choose(price_p[1])
    run_stop()

    turn_light(0, animation.idle)

    pressed_key = get_key()
    if pressed_key == button_cash then return mode_fundraising end
    if pressed_key == button_cashless then return mode_keyboard end
    
    update_balance()
    if balance > 0.99 then return mode_fundraising end

    return mode_choose
end

keyboard_mode = function()
    if keyboard_pressed then show_keyboard(electron_balance)
    else show_keyboard(min_electron_balance) end
    run_stop()

    turn_light(0, animation.idle)

    pressed_key = get_key()
    if pressed_key >= 1 and pressed_key <= 10 then
        keyboard_pressed = true
        electron_balance = electron_balance * 10 + pressed_key - 1
        if electron_balance > max_electron_balance then electron_balance = max_electron_balance end
    elseif pressed_key == button_cancel then
        keyboard_pressed = false
        electron_balance = 0
        return mode_choose
    elseif pressed_key == button_begin then
        if keyboard_pressed == false then electron_balance = min_electron_balance end
        keyboard_pressed = false
        return mode_wait
    end

    update_balance()
    if balance > 0.99 then return mode_fundraising end
    
    return mode_keyboard
end

wait_mode = function()
    show_wait(electron_balance)
    run_stop()

    turn_light(0, animation.idle)

    if is_transaction_started == false then
        waiting_loops = wait_mode_seconds * 10;

        request_transaction(electron_balance)
        electron_balance = min_electron_balance
        is_transaction_started = true
    end

    pressed_key = get_key()
    if pressed_key == button_cancel then waiting_loops = 0 end

    status = get_transaction_status()
    update_balance()

    if balance > 0.99 then
        if status ~= 0 then abort_transaction() end
        is_transaction_started = false
        return mode_start_filling
    end

    if waiting_loops <= 0 then
        is_transaction_started = false
	    if status ~= 0 then abort_transaction() end
        return mode_choose
    end

    if (status == 0) and (is_transaction_started == true) then
        is_transaction_started = false
        abort_transaction()
        return mode_choose       
	end

    smart_delay(100)
    waiting_loops = waiting_loops - 1
   
    return mode_wait
end

fundraising_mode = function()
    show_fundraising(balance)
    run_stop()
    turn_light(0, animation.idle)
    init_prices()
    
    pressed_key = get_key()
    if pressed_key == button_cancel then return mode_choose end
    if pressed_key == button_begin then return mode_start_filling end

    update_balance()

    return mode_fundraising
end

start_filling_mode = function()
    volume = balance / price_p[1]
    show_start_filling(balance, volume)
    run_stop()
    turn_light(0, animation.idle)
    
    pressed_key = get_key()
    if pressed_key == button_begin then return mode_filling end

    update_balance()

    return mode_start_filling
end

filling_mode = function()
  run_fillin()
  show_filling(balance)
  
  -- тут тарн лайт
  
  charge_balance(price_p[1])

  if balance <= 0.01 then return mode_thanks end

  update_balance()

  return mode_filling
end

thanks_mode = function()
    run_stop()
    show_thanks()

    if is_waiting_receipt == false then
        balance = 0
        turn_light(1, animation.one_button)
        waiting_loops = thanks_mode_seconds * 10;
        is_waiting_receipt = true
    end
 
    if waiting_loops > 0 then
        pressed_key = get_key()
        if pressed_key == button_cancel then waiting_loops = 0 end

        update_balance()
        if balance > 0.99 then
            send_receipt(post_position, cash_balance, electronical_balance)
            is_waiting_receipt = false
            increment_cars() 
            return mode_fundraising
        end
        waiting_loops = waiting_loops - 1
    
    else
        send_receipt(post_position, cash_balance, electronical_balance)
        is_waiting_receipt = false
        increment_cars()
	    return mode_choose
    end

    return mode_thanks
end

-- Show

show_welcome = function()
    welcome:Display()
end

show_choose = function(price_rur)
    choose:Set("price.value", math.ceil(price_rur))
    if hascardreader() then choose:Set("button_cashless.visible", "true")
    else choose:Set("button_cashless.visible", "false") end
    choose:Display()
end

show_fundraising = function(balance_rur)
    balance_int = math.ceil(balance_rur)
    fundraising:Set("balance.value", balance_int)
    if balance_int >= 1 then fundraising:Set("button_begin.visible", "true")
    else fundraising:Set("button_begin.visible", "false") end
    fundraising:Display()
end

show_keyboard = function(balance_rur)
    balance_int = math.ceil(balance_rur)
    keyboard:Set("balance.value", balance_int)
    if (keyboard_pressed and balance_int >= min_electron_balance) or keyboard_pressed == false then
        keyboard:Set("button_begin.visible", "true")
    else
        keyboard:Set("button_begin.visible", "false")
    end
    keyboard:Display()
end

show_wait = function(balance_rur)
    balance_int = math.ceil(balance_rur)
    wait:Set("balance.value", balance_int)
    wait:Display()
end

show_start_filling = function(balance_rur, volume_rur)
    volume_1, volume_2 = math.modf(volume_rur)
    start_filling:Set("balance.value", math.ceil(balance_rur))
    start_filling:Set("volume_1.value", volume_1)
    start_filling:Set("volume_2.value", math.ceil(volume_2 * 100))
    start_filling:Display()
end

show_filling = function(balance_rur)
    balance_int = math.ceil(balance_rur)
    filling:Set("balance.value", balance_int)
    filling:Display()
end

show_thanks =  function()
    thanks:Display()
end

show_apology =  function()
    apology:Display()
end

-- Util

get_key = function()
    return hardware:GetKey()
end

smart_delay = function(ms)
    return hardware:SmartDelay(ms)
end

get_price = function(key)
    return registry:GetPrice(key)
end

turn_light = function(rel_num, animation_code)
    hardware:TurnLight(rel_num, animation_code)
end

send_receipt = function(post_pos, cash, electronical)
    hardware:SendReceipt(post_pos, cash, electronical)
    cash_balance = 0
    electronical_balance = 0
end

increment_cars = function()
    hardware:IncrementCars()
end

run_fillin = function()
    set_current_state(balance, 1)
    run_program(1)
end

run_stop = function()
    set_current_state(balance, 0)
    run_program(0)
end

run_program = function(program_num)
    hardware:TurnProgram(program_num)
end

request_transaction = function(money)
    return hardware:RequestTransaction(money)
end

get_transaction_status = function()
    return hardware:GetTransactionStatus()
end

abort_transaction = function()
    return hardware:AbortTransaction()
end

set_current_state = function(current_balance, current_program)
    return hardware:SetCurrentState(math.floor(current_balance), current_program)
end

update_balance = function()
    new_coins = hardware:GetCoins()
    new_banknotes = hardware:GetBanknotes()
    new_electronical = hardware:GetElectronical()
    new_service = hardware:GetService()

    cash_balance = cash_balance + new_coins
    cash_balance = cash_balance + new_banknotes
    electronical_balance = electronical_balance + new_electronical
    balance = balance + new_coins
    balance = balance + new_banknotes
    balance = balance + new_electronical
    balance = balance + new_service
end

charge_balance = function(price)
    balance = balance - price * real_ms_per_loop / 60000
    if balance<0 then balance = 0 end
end

forget_pressed_key = function()
    key = get_key()
end

hascardreader = function()
  return hardware:HasCardReader()
end
