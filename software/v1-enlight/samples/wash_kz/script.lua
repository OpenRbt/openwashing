-- Wash Firmware

-- setup is running at the start just once
setup = function()
    -- global variables
    balance = 0.0

    -- program to turn on when user paid money but has not selected a program
    default_paid_program = 6

    min_electron_balance = 50
    max_electron_balance = 900
    electron_amount_step = 25
    electron_balance = min_electron_balance
    
    balance_seconds = 0
    cash_balance = 0.0
    electronical_balance = 0.0
    post_position = 1      

    -- constants
    welcome_mode_seconds = 3
    thanks_mode_seconds = 120
    free_pause_seconds = 120
    wait_card_mode_seconds = 40
    
    is_transaction_started = false
    is_waiting_receipt = false

    price_p = {}
    
    price_p[0] = 0
    price_p[1] = 0
    price_p[2] = 0
    price_p[3] = 0
    price_p[4] = 0
    price_p[5] = 0
    price_p[6] = 0
    price_p[7] = 0

    init_prices()
    
    mode_welcome = 0
    mode_choose_method = 10
    mode_select_price = 20
    mode_wait_for_card = 30
    mode_ask_for_money = 40
    
    -- all these modes MUST follow each other
    mode_work = 50
    mode_pause = 60
    -- end of modes which MUST follow each other
    
    mode_thanks = 120
    
    currentMode = mode_welcome

    version = "2.2.1"

    printMessage("dia generic wash firmware v." .. version)
    return 0
end

-- loop is being executed
loop = function()
    currentMode = run_mode(currentMode)
    smart_delay(100)
    return 0
end

init_prices = function()
    price_p[1] = 80
    
    price_p[2] = 95
    
    price_p[3] = 70
    
    price_p[4] = 95
    
    price_p[5] = 70
    
    price_p[6] = 50
    price_p[7] = 130
    
    --price_p[7] = get_price("price7")
    --if price_p[7] == 0 then price_p[7] = 18 end
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
    rubbish = get_key()
    pressed_key = -1
    smart_delay(1000 * welcome_mode_seconds)
    forget_pressed_key()
    if hascardreader() then
        return mode_choose_method
    end    
    return mode_ask_for_money
end

choose_method_mode = function()
    show_choose_method()
    run_stop()

    -- check animation
    turn_light(0, animation.idle)

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
  show_working(sub_mode, balance)
  
  if sub_mode == 0 then
    run_program(default_paid_program)
    balance_seconds = free_pause_seconds
    turn_light(0, animation.intense)
  else
    run_program(sub_mode)
    turn_light(sub_mode, animation.one_button)
  end
  
  charge_balance(price_p[sub_mode])
  set_current_state(balance,sub_mode)
  if balance <= 0.01 then 
    return mode_thanks 
  end
  update_balance()
  suggested_mode = get_mode_by_pressed_key()
  if suggested_mode >=0 then return suggested_mode end
  return working_mode
end

pause_mode = function()
    show_pause(balance, balance_seconds)
    run_pause()
    turn_light(6, animation.one_button)
    update_balance()
    if balance_seconds > 0 then
        balance_seconds = balance_seconds - 0.1
    else
        balance_seconds = 0
        charge_balance(price_p[6])
    end
    
    set_current_state(balance,6)  
    
    if balance <= 0.01 then return mode_thanks end
    
    suggested_mode = get_mode_by_pressed_key()
    if suggested_mode >=0 then return suggested_mode end
    return mode_pause
end

thanks_mode = function()
    set_current_state(0,0)
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
            send_receipt(post_position, cash_balance, electronical_balance)
            cash_balance = 0
            electronical_balance = 0
            is_waiting_receipt = false
            increment_cars() 
            return mode_work 
        end
        waiting_loops = waiting_loops - 1
    else
        send_receipt(post_position, cash_balance, electronical_balance)
        cash_balance = 0
        electronical_balance = 0
        is_waiting_receipt = false
        increment_cars()
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
    if hascardreader() then
        ask_for_money:Set("return_background.visible", "true")
    end
    ask_for_money:Display()
end

show_choose_method = function()
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

show_working = function(sub_mode, balance_rur)
    balance_int = math.ceil(balance_rur)
    working:Set("pause_digits.visible", "false")
    working:Set("balance.value", balance_int)
    
    switch_submodes(sub_mode)
    working:Display()
end

show_pause = function(balance_rur, balance_sec)
    balance_int = math.ceil(balance_rur)
    sec_int = math.ceil(balance_sec)
    working:Set("pause_digits.visible", "true")
    working:Set("pause_digits.value", sec_int)
    working:Set("balance.value", balance_int)
    switch_submodes(6)
    working:Display()
end

switch_submodes = function(sub_mode)
    working:Set("cur_p.index", sub_mode-1) 
end

show_thanks =  function(seconds_float)
    seconds_int = math.ceil(seconds_float)
    thanks:Set("delay_seconds.value", seconds_int)
    thanks:Display()
end

get_mode_by_pressed_key = function()
    pressed_key = get_key()
    if pressed_key >= 1 and pressed_key<=5 then return mode_work + pressed_key end
    if pressed_key == 6 then return mode_pause end
    if pressed_key == 7 then return mode_work + pressed_key end
    return -1
end

get_key = function()
    return hardware:GetKey()
end

smart_delay = function(ms)
    return hardware:SmartDelay(ms)
end

get_price = function(key)
    return registry:ValueInt(key)
end

turn_light = function(rel_num, animation_code)
    hardware:TurnLight(rel_num, animation_code)
end

send_receipt = function(post_pos, cash, electronical)
    hardware:SendReceipt(post_pos, cash, electronical)
end

increment_cars = function()
    hardware:IncrementCars()
end

run_pause = function()
    run_program(program.p6relay)
end

run_stop = function()
    run_program(program.stop)
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
    balance = balance - price * 0.001666666667
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
  