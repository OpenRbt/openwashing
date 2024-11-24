-- Wash Firmware, vertical, temp


-- current programs 
-- button 1 p1 p2
-- button 2 p3 p4
-- button 3 p5
-- button 4 p6
-- button 5 p7 // used as pause

-- setup is running at the start just once
setup = function()
    -- global variables
    balance = 0.0

    -- program to turn on when user paid money but has not selected a program
    default_paid_program = 7

    min_electron_balance = 50
    max_electron_balance = 900
    electron_amount_step = 25
    electron_balance = min_electron_balance
    set_l2()
    
    balance_seconds = 0
    cash_balance = 0.0
    electronical_balance = 0.0
    post_position = 1
    money_wait_seconds = 0
    last_program_id = 0

        -- time variables
    time_minutes = 25
    time_hours = 13

    -- temperature variables
    temp_degrees = 6
    temp_is_negative = true

    -- constants
    thanks_mode_seconds = 120
    free_pause_seconds = 120
    wait_card_mode_seconds = 40
    max_money_wait_seconds = 90
    
    is_transaction_started = false
    is_waiting_receipt = false
    is_money_added = false

    price_p = {}
    
    price_p[0] = 0
    price_p[1] = 0
    price_p[2] = 0
    price_p[3] = 0
    price_p[4] = 0
    price_p[5] = 0
    price_p[6] = 0
    price_p[7] = 0
    
    discount_p = {}
    
    discount_p[0] = 0
    discount_p[1] = 0
    discount_p[2] = 0
    discount_p[3] = 0
    discount_p[4] = 0
    discount_p[5] = 0
    discount_p[6] = 0
    discount_p[7] = 0

    init_prices()
    init_discounts()

    mode_welcome = 0
    mode_choose_method = 100
    mode_select_price = 200
    mode_wait_for_card = 300
    mode_ask_for_money = 400
    
    -- all these modes MUST follow each other
    mode_work = 500
    mode_pause = 600
    -- end of modes which MUST follow each other
    
    mode_thanks = 1200
    real_ms_per_loop = 100
    
    currentMode = mode_welcome

    version = "2.3.0"

    printMessage("dia test firmware for vertical screens v." .. version)
    -- external constants
    init_constants();
    update_post();
    welcome:Set("post_number.value", post_position)
	welcome:Set("post_number_2.value", post_position) -- later we might delete post_number_2
    forget_pressed_key();
    return 0
end

-- loop is being executed
loop = function()
    update_post()

    if balance < 0.1 and money_wait_seconds > 0 then
        money_wait_seconds = money_wait_seconds - 1
    end
    if is_money_added and money_wait_seconds <= 0 and get_is_finishing_programm(last_program_id) then
        increment_cars()
        is_money_added = false
        last_program_id = 0
    end

    currentMode = run_mode(currentMode)
    real_ms_per_loop = smart_delay(100)
    return 0
end

set_l3 = function()
    working = working_l3
    ask_for_money = ask_for_money_l3
    thanks = thanks_l3
end

set_l2 = function()
    working = working_l2
    ask_for_money = ask_for_money_l2
    thanks = thanks_l2
end

set_l1 = function()
    working = working_l1
    ask_for_money = ask_for_money_l1
    thanks = thanks_l1
end

update_time = function()
    time_minutes = hardware:GetMinutes()
    time_hours = hardware:GetHours()
end

update_temp = function()
    temp_degrees = weather:GetTempDegrees()
    temp_is_negative = weather:IsNegative()
    working:Set("temp_degrees.value", temp_degrees)
  
    if temp_is_negative then
        working:Set("temp_sign.visible", "true") 
    else
        working:Set("temp_sign.visible", "false") 
    end
end

click_id_by_button = function (btn, cur_program)
    if btn == 0 then
        return 0
    end
    if btn == 1 then
        return 1
    end
    if btn == 2 then
        return 2
    end
    if btn == 3 then
        return 3
    end
    if btn == 4 then
        return 4
    end
    if btn == 5 then 
        return 5
    end
end

update_post = function() 
    post_position = registry:GetPostID();
end

init_prices = function()
    price_p[1] = get_price(1)
    price_p[2] = get_price(2)
    price_p[3] = get_price(3)
    price_p[4] = get_price(4)
    price_p[5] = get_price(5)
    price_p[6] = get_price(6)
    price_p[7] = get_price(7)
end

init_discounts = function()
    discount_p[1] = get_discount(1)
    discount_p[2] = get_discount(2)
    discount_p[3] = get_discount(3)
    discount_p[4] = get_discount(4)
    discount_p[5] = get_discount(5)
    discount_p[6] = get_discount(6)
    discount_p[7] = get_discount(7)
end


run_mode = function(new_mode)   
    if new_mode == mode_welcome then return welcome_mode() end
    if new_mode == mode_choose_method then return choose_method_mode() end
    if new_mode == mode_select_price then return select_price_mode() end
    if new_mode == mode_wait_for_card then return wait_for_card_mode() end
    if new_mode == mode_ask_for_money then return ask_for_money_mode() end
    
    if is_working_mode (new_mode) then return program_mode(new_mode) end
    if new_mode == mode_pause then return pause_mode() end
    
    if new_mode == mode_thanks then return thanks_mode() end
end

welcome_mode = function()
    show_welcome()
    run_stop()
    turn_light(0, animation.idle)
    init_prices()
    update_balance()
    balance_int = math.ceil(balance)
    welcome:Set("balance.value", balance_int)
    init_discounts()
    pressed_key = get_key()
    return mode_ask_for_money
end

choose_method_mode = function() -- not reviewed properly
    show_choose_method()
    run_stop()

    -- check animation
    turn_light(0, animation.idle)

    init_prices()
    init_discounts()
    
    pressed_key = get_key()
    if pressed_key == 4 or pressed_key == 5 or pressed_key == 6 then
        return mode_select_price
    end
    if pressed_key == 1 or pressed_key == 2 or pressed_key == 3 then
        return mode_ask_for_money
    end
    
    -- if someone put some money let's switch the mode.
    -- this should be rebuilt
    update_balance()
    if balance > 0.99 then
        return mode_work
    end

    return mode_choose_method
end

select_price_mode = function()
    show_select_price(electron_balance)
    run_stop()

    -- check animation
    turn_light(0, animation.idle)

    pressed_key = get_key()
    -- increase amount
    if pressed_key == 1 then
        electron_balance = electron_balance + electron_amount_step
        if electron_balance >= max_electron_balance then
            electron_balance = max_electron_balance
        end
    end
    -- decrease amount
    if pressed_key == 2 then
        electron_balance = electron_balance - electron_amount_step
        if electron_balance <= min_electron_balance then
            electron_balance = min_electron_balance
        end 
    end
    --return to choose method
    if pressed_key == 3 then
        return mode_choose_method
    end
    if pressed_key == 6 then
        return mode_wait_for_card
    end

    return mode_select_price
end

wait_for_card_mode = function()
    show_wait_for_card()
    run_stop()

    -- check animation
    turn_light(0, animation.idle)

    if is_transaction_started == false then
        waiting_loops = wait_card_mode_seconds * 10;

        request_transaction(electron_balance)
        electron_balance = min_electron_balance
        is_transaction_started = true
    end

    pressed_key = get_key()
    if pressed_key > 0 and pressed_key < 7 then
        waiting_loops = 0
    end

    status = get_transaction_status()
    update_balance()

    if balance > 0.99 then
        if status ~= 0 then 
            abort_transaction()
        end
        is_transaction_started = false
        return mode_work
    end

    if waiting_loops <= 0 then
    is_transaction_started = false
	if status ~= 0 then
	    abort_transaction()
	end
        return mode_choose_method
    end

    if (status == 0) and (is_transaction_started == true) then
        is_transaction_started = false
        abort_transaction()
        return mode_choose_method        
	end

    smart_delay(100)
    waiting_loops = waiting_loops - 1
   
    return mode_wait_for_card
end

ask_for_money_mode = function()
    show_ask_for_money()
    run_stop()
    turn_light(0, animation.idle)
    pressed_key = get_key()
    if pressed_key >= 1 and  pressed_key <= 3 then 
        forget_pressed_key()
        return mode_welcome
    end
    
    if hascardreader() then
        pressed_key = get_key()
        if pressed_key > 0 and pressed_key < 7 then
            return mode_choose_method
        end
    else
        forget_pressed_key()
    end
    

    update_balance()
    if balance > 0.99 then
        return mode_work
    end
    return mode_ask_for_money
end

program_mode = function(working_mode)
  sub_mode = working_mode - mode_work
  cur_price = price_p[sub_mode]
  update_time()
  update_temp()
  show_working(sub_mode, balance, cur_price)
  show_active_ico(sub_mode)
  
  if sub_mode == 0 then
    run_program(default_paid_program)
    balance_seconds = free_pause_seconds
    turn_light(0, animation.intense)
  else
    run_sub_program(sub_mode)
    turn_light(sub_mode, animation.one_button)
  end
  if get_is_preflight() == 0 then
    charge_balance(price_p[sub_mode])
  end
  set_current_state(balance)
  if balance <= 0.01 then
    return mode_thanks 
  end
  update_balance()
  suggested_mode = get_mode_by_pressed_key(sub_mode)
  
  if suggested_mode >=0 then return suggested_mode end
  return working_mode
end

run_sub_program = function(program_index)
    run_program(program_index)
end

pause_mode = function()
    
    run_pause()
    turn_light(5, animation.one_button)
    update_balance()
    cur_price = 0
    if balance_seconds > 0 then
        balance_seconds = balance_seconds - 0.1
    else
        balance_seconds = 0
        charge_balance(price_p[5])
        cur_price = price_p[5]
    end
    set_current_state(balance, 5)
    show_pause(balance, balance_seconds, cur_price)
    
    if balance <= 0.01 then return mode_thanks end
    
    suggested_mode = get_mode_by_pressed_key(5)
    if suggested_mode >=0 then return suggested_mode end
    return mode_pause
end

thanks_mode = function()
    set_current_state(0)
    if is_waiting_receipt == false then
        balance = 0
        show_thanks(thanks_mode_seconds)
        turn_light(1, animation.one_button)
        run_pause()
        waiting_loops = thanks_mode_seconds * 10;
        is_waiting_receipt = true
    end
 
    if waiting_loops > 0 then
        show_thanks(waiting_loops/10)
        pressed_key = get_key()
        if pressed_key > 0 and pressed_key < 7 then
            waiting_loops = 0
        end
        update_balance()
        if balance > 0.99 then
            is_waiting_receipt = false
            return mode_work 
        end
        waiting_loops = waiting_loops - 1
    else
        send_receipt(post_position, cash_balance, electronical_balance)
        cash_balance = 0
        electronical_balance = 0
        is_waiting_receipt = false
	if hascardreader() then
        	return mode_choose_method
    	end
        return mode_ask_for_money
    end

    return mode_thanks
end

show_welcome = function()
    welcome:Display()
end

show_ask_for_money = function()
    ask_for_money:Set("post_number.value", post_position)
    if hascardreader() then
        ask_for_money:Set("return_background.visible", "true")
    end
    ask_for_money:Display()
end

show_choose_method = function()
    choose_method:Set("post_number.value", post_position)
    choose_method:Display()
end

show_select_price = function(balance_rur)
    balance_int = math.ceil(balance_rur)
    select_price:Set("balance.value", balance_int)
    select_price:Display()
end

show_wait_for_card = function()
    wait_for_card:Display()
end

show_start = function(balance_rur)
    balance_int = math.ceil(balance_rur)
    start:Set("balance.value", balance_int)
    start:Display()
end

show_working = function(sub_mode, balance_rur, price_rur)
    balance_int = math.ceil(balance_rur)
    working:Set("pause_digits.visible", "false")
    working:Set("balance.value", balance_int)
    working:Set("price.value", price_rur)
    working:Set("time_min.value", time_minutes)
    working:Set("time_hours.value", time_hours)
    switch_submodes(sub_mode)
    working:Display()
end
show_active_ico  = function(sub_mode)
    if sub_mode == 1 then
        working:Set("p1_a_ico.visible", "true")
    else 
        working:Set("p1_a_ico.visible", "false")
    end
    if sub_mode == 2 then
        working:Set("p2_a_ico.visible", "true")
    else 
        working:Set("p2_a_ico.visible", "false")
    end
    if sub_mode == 3 then
        working:Set("p3_a_ico.visible", "true")
    else 
        working:Set("p3_a_ico.visible", "false")
    end
    if sub_mode == 4 then
        working:Set("p4_a_ico.visible", "true")
    else 
        working:Set("p4_a_ico.visible", "false")
    end
    if sub_mode == 5 then
        working:Set("p5_a_ico.visible", "true")
    else 
        working:Set("p5_a_ico.visible", "false")
    end
end

show_pause = function(balance_rur, balance_sec, price_rur)
    balance_int = math.ceil(balance_rur)
    sec_int = math.ceil(balance_sec)
    working:Set("pause_digits.visible", "true")
    working:Set("pause_digits.value", sec_int)
    working:Set("balance.value", balance_int)
    working:Set("price.value", price_rur)
    switch_submodes(5)
    show_active_ico(5)
    working:Display()
end

switch_submodes = function(sub_mode)
    working:Set("cur_p.index", sub_mode) 
    working:Set("cur_tip.index", sub_mode) 
end

show_thanks =  function(seconds_float)
    seconds_int = math.ceil(seconds_float)
    thanks:Set("delay_seconds.value", seconds_int)
    thanks:Display()
end

get_mode_by_pressed_key = function(sub_mode)
    pressed_key = get_key()
    virtual_key = click_id_by_button(pressed_key, sub_mode)
    if virtual_key >= 1 and virtual_key<=4 then return mode_work + virtual_key end
    if virtual_key == 5 then return mode_pause end
    return -1
end

get_key = function()
    return hardware:GetKey()
end

smart_delay = function(ms)
    return hardware:SmartDelay(ms)
end

get_price = function(key)
    return registry:GetPrice(key)
end

get_discount = function(key)
    return registry:GetDiscount(key)
end

set_value_if_not_exists = function(key, value)
    return registry:SetValueByKeyIfNotExists(key, value)
end

turn_light = function(rel_num, animation_code)
    hardware:TurnLight(rel_num, animation_code)
end

send_receipt = function(post_pos, cash, electronical)
    hardware:SendReceipt(post_pos, cash, electronical, 0)
end

increment_cars = function()
    hardware:IncrementCars()
end

run_pause = function()
    run_program(5)
end

run_stop = function()
    run_program(-1)
end

run_program = function(program_num)
    if program_num ~= 7 and program_num >0 then
        last_program_id = program_num
    end
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

set_current_state = function(current_balance)
    return hardware:SetCurrentState(math.floor(current_balance))
end

get_is_preflight = function()
    return hardware:GetIsPreflight()
end

get_is_finishing_programm = function(program_id)
    if registry:GetIsFinishingProgram(program_id) == 1 then
        return true
    end
    return false
end

update_balance = function()
    new_coins = hardware:GetCoins()
    new_banknotes = hardware:GetBanknotes()
    new_electronical = hardware:GetElectronical()
    new_service = hardware:GetService()
    new_sbp = hardware:GetSbpMoney()

    if  new_sbp >0 or new_coins > 0 or new_banknotes > 0 or new_electronical > 0 or new_service > 0 then
        is_money_added = true
        money_wait_seconds = max_money_wait_seconds * 10
    end

    cash_balance = cash_balance + new_coins
    cash_balance = cash_balance + new_banknotes
  electronical_balance = electronical_balance + new_electronical + new_sbp
    balance = balance + new_coins
    balance = balance + new_banknotes
    balance = balance + new_electronical
    balance = balance + new_service
    balance = balance + new_sbp    
end

charge_balance = function(price)
    balance = balance - price * real_ms_per_loop / 60000
    if balance<0 then balance = 0 end
end

is_working_mode = function(mode_to_check)
  if mode_to_check >= mode_work and mode_to_check<mode_work+10 then return true end
  return false
end

forget_pressed_key = function()
    key = get_key()
end

hascardreader = function()
  return hardware:HasCardReader()
end
