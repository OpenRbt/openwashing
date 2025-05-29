-- Wash Firmware

BlockState = {
    NOT_BLOCKED = 0,
    TTL_EXPIRED = 1,
    NO_SERVER_CONNECTION = 2
}

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

    sbp_balance = 50
    
    balance_seconds = 0
    cash_balance = 0.0
    electronical_balance = 0.0
    sbp_receipt_balance = 0.0
    bonuses_balance = 0.0
    post_position = 1
    money_wait_seconds = 0
    last_program_id = 0

    -- constants
    welcome_mode_seconds = 3
    thanks_mode_seconds = 120
    before_start_work_seconds = 60

    connection_to_bonus_seconds = 5
    wait_for_QR_seconds = 15
    free_pause_seconds = 120
    wait_card_mode_seconds = 40
    max_money_wait_seconds = 90
    
    is_paused = false
    is_at_start = true
    is_authorized = false
    is_transaction_started = false
    is_connected_to_bonus_system = false
    is_waiting_receipt = false
    is_money_added = false
    is_connected_to_sbp = false

    price_p = {}
    
    price_p[0] = 0
    price_p[1] = 0
    price_p[2] = 0
    price_p[3] = 0
    price_p[4] = 0
    price_p[5] = 0
    price_p[6] = 0
    
    discount_p = {}
    
    discount_p[0] = 0
    discount_p[1] = 0
    discount_p[2] = 0
    discount_p[3] = 0
    discount_p[4] = 0
    discount_p[5] = 0
    discount_p[6] = 0

    init_prices()
    init_discounts()

    mode_welcome = 0
    mode_choose_method = 10
    mode_select_price = 20
    mode_wait_for_card = 30
    mode_ask_for_money = 40
    mode_sbp_select_price = 41
    mode_sbp_payment = 42
    mode_wait_for_QR = 43
    mode_sorry = 44
    
    -- all these modes MUST follow each other
    mode_work = 50
    mode_pause = 60
    
    -- end of modes which MUST follow each other
    mode_confirm_end = 110
    mode_thanks = 120
    mode_connected_to_bonus_system = 130
    mode_no_internet = 140
    
    real_ms_per_loop = 100
    
    currentMode = mode_welcome

    version = "2.2.0"

    printMessage("dia generic wash firmware v." .. version)
    -- external constants
    init_constants();
    update_post();
    welcome:Set("post_number.value", post_position)
    
    create_session()

    qr = "";
    sbpQr = "";
    sbpQrTemp = "";
    visible_session = "";
    
    forget_pressed_key();

    is_system_blocked = false
    block_state = BlockState.NOT_BLOCKED

    return 0
end

-- loop is being executed
loop = function()
    block_state = hardware:GetBlockState()
    is_system_blocked = block_state ~= BlockState.NOT_BLOCKED

    if is_system_blocked then
        if block_state == BlockState.TTL_EXPIRED or block_state == BlockState.NO_SERVER_CONNECTION then
            currentMode = mode_no_internet
        end
    end

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
end

init_discounts = function()
    discount_p[1] = get_discount(1)
    discount_p[2] = get_discount(2)
    discount_p[3] = get_discount(3)
    discount_p[4] = get_discount(4)
    discount_p[5] = get_discount(5)
    discount_p[6] = get_discount(6)
end


run_mode = function(new_mode)   
    if new_mode == mode_welcome then return welcome_mode() end
    if new_mode == mode_choose_method then return choose_method_mode() end
    if new_mode == mode_select_price then return select_price_mode() end
    if new_mode == mode_wait_for_card then return wait_for_card_mode() end
    if new_mode == mode_ask_for_money then return ask_for_money_mode() end
    if new_mode == mode_sbp_select_price then return sbp_select_price_mode() end
    if new_mode == mode_sbp_payment then return sbp_payment_mode() end
    if new_mode == mode_wait_for_QR then return wait_for_QR_mode() end
    if new_mode == mode_sorry then return sorry_mode() end
    
    if is_working_mode (new_mode) then return program_mode(new_mode) end
    if new_mode == mode_pause then return pause_mode() end
    if new_mode == mode_confirm_end then return confirm_end_mode() end
    if new_mode == mode_connected_to_bonus_system then return connected_to_bonus_system_mode() end
    
    if new_mode == mode_thanks then return thanks_mode() end
    if new_mode == mode_no_internet then return no_internet_mode() end
end

welcome_mode = function()
    show_welcome()
    run_stop()
    turn_light(0, animation.idle)
    init_prices()
    init_discounts()
    smart_delay(1000 * welcome_mode_seconds)
    forget_pressed_key()
    
    return mode_choose_method
end

choose_method_mode = function()
    visible_session = hardware:GetVisibleSession();
    is_connected_to_sbp = get_is_connected_to_sbp_system()
    get_QR()
    
    show_choose_method()
    run_stop()

    -- check animation
    turn_light(0, animation.idle)

    init_prices()
    init_discounts()
    
    pressed_key = get_key()

    if hascardreader() and (pressed_key == 5 or pressed_key == 6) then
        return mode_select_price
    end
    if  pressed_key == 1 or pressed_key == 2 then
        return mode_ask_for_money
    end
    if is_connected_to_sbp and (pressed_key == 3 or pressed_key == 4) then
        return mode_sbp_select_price
    end
    
    
    -- if someone put some money let's switch the mode.
    -- this should be rebuilt
    update_balance()
    if balance > 0.99 then
        return mode_work
    end
    
    if get_is_connected_to_bonus_system() then
        return mode_connected_to_bonus_system
    end

    return mode_choose_method
end

connected_to_bonus_system_mode = function()

    show_connected_to_bonus_system()
    if is_connected_to_bonus_system == false then
        waiting_seconds = connection_to_bonus_seconds * 10;
        is_connected_to_bonus_system = true
    end

    if waiting_seconds > 0 then
        pressed_key = get_key()
        if pressed_key > 0 and pressed_key < 7 then
            waiting_seconds = 0
        end
        waiting_seconds = waiting_seconds - 1
    else
        is_connected_to_bonus_system = false
        set_is_connected_to_bonus_system(false)
        return mode_choose_method
    end

    return mode_connected_to_bonus_system
end

select_price_mode = function()
    is_connected_to_bonus_system = false
    set_is_connected_to_bonus_system(false)
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
    is_connected_to_bonus_system = false
    set_is_connected_to_bonus_system(false)
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
    is_connected_to_bonus_system = false
    set_is_connected_to_bonus_system(false)
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

sbp_select_price_mode = function()
    is_connected_to_bonus_system = false
    set_is_connected_to_bonus_system(false)    
    show_sbp_select_price(sbp_balance)
    run_stop()

    -- check animation
    turn_light(0, animation.idle)

    pressed_key = get_key()
    -- increase amount
    if pressed_key == 1 then
        sbp_balance = sbp_balance + electron_amount_step
        if sbp_balance >= max_electron_balance then
            sbp_balance = max_electron_balance
        end
    end
    -- decrease amount
    if pressed_key == 2 then
        sbp_balance = sbp_balance - electron_amount_step
        if sbp_balance <= min_electron_balance then
            sbp_balance = min_electron_balance
        end 
    end
    --return to choose method
    if pressed_key == 3 then
        return mode_choose_method
    end
    if pressed_key == 6 then
        hardware:CreateSbpPayment(sbp_balance)
        sbp_balance = min_electron_balance
        waiting_loops = wait_for_QR_seconds * 10;
        return mode_wait_for_QR
    end

    return mode_sbp_select_price
end

wait_for_QR_mode = function()
    is_connected_to_bonus_system = false
    set_is_connected_to_bonus_system(false)
    run_stop()
    get_sbp_qr()
    pressed_key = get_key()
    if sbpQr == '' or sbpQr == nil or sbpQr == sbpQrTemp then
        if waiting_loops > 0 then
            show_wait_for_QR(waiting_loops/10)
            waiting_loops = waiting_loops - 1
        else
            waiting_loops = 10 * 10
            return mode_sorry
        end
    else
        sbpQrTemp = sbpQr
        return mode_sbp_payment
    end

    return mode_wait_for_QR
end

sorry_mode = function()
    is_connected_to_bonus_system = false
    set_is_connected_to_bonus_system(false)
    run_stop()
    show_sorry()

    pressed_key = get_key()
    if pressed_key > 0 and pressed_key < 7 then
        return mode_choose_method
    end
    
    if waiting_loops > 0 then
        waiting_loops = waiting_loops - 1
    else
        return mode_choose_method
    end

    return mode_sorry
end

sbp_payment_mode = function()
    is_connected_to_bonus_system = false
    set_is_connected_to_bonus_system(false)

    get_sbp_qr()
    if sbpQr == nil or sbpQr == '' then
        return mode_choose_method
    end

    show_sbp_payment()
    run_stop()

    turn_light(0, animation.idle)

    pressed_key = get_key()

    if pressed_key > 0 and pressed_key < 7 then
        return mode_choose_method
    end

    update_balance()
    if balance > 0.99 then
        return mode_work
    end

    return mode_sbp_payment
end

program_mode = function(working_mode)
    is_paused = false
  sub_mode = working_mode - mode_work
  cur_price = price_p[sub_mode]
  show_working(sub_mode, balance, cur_price)
  
  if sub_mode == 0 then
    run_program(default_paid_program)

    if is_at_start == true then 
        balance_seconds = free_pause_seconds
        start_countdown = before_start_work_seconds * 10
        is_at_start = false
    end
 
    if start_countdown > 0 then
        start_countdown = start_countdown - 1
    else
        run_pause()
        return mode_pause
    end
    
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
    forget_pressed_key()
    return mode_thanks 
  end
  update_balance()
  suggested_mode = get_mode_by_pressed_key()
  if suggested_mode >=0 then return suggested_mode end
  return working_mode
end

run_sub_program = function(program_index)
    run_program(program_index)
end

pause_mode = function()
    
    is_paused = true
    run_pause()
    turn_light(6, animation.one_button)
    update_balance()
    cur_price = 0
    if balance_seconds > 0 then
        balance_seconds = balance_seconds - 0.1
    else
        balance_seconds = 0
        charge_balance(price_p[6])
        cur_price = price_p[6]
    end
    set_current_state(balance,6)
    show_pause(balance, balance_seconds, cur_price)
    
    if balance <= 0.01 then
        forget_pressed_key()
        return mode_thanks 
    end
    
    suggested_mode = get_mode_by_pressed_key()

    if suggested_mode == 60 and is_authorized_function() then
        return mode_confirm_end 
    end

    if suggested_mode >=0 then return suggested_mode end
    return mode_pause
end

confirm_end_mode = function ()
    show_confirm_end()
    
    pressed_key = get_key()
    if pressed_key == 1 then
        return mode_pause
    end

    if pressed_key == 6 then
        hardware:SetBonuses(math.ceil(balance))
        money_wait_seconds = 0
        forget_pressed_key()
        return mode_thanks
    end

    return mode_confirm_end
end

thanks_mode = function()
    is_connected_to_bonus_system = false
    set_is_connected_to_bonus_system(false)
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
            is_at_start = true
            return mode_work 
        end
        waiting_loops = waiting_loops - 1
    else
        send_receipt(post_position, cash_balance, electronical_balance, sbp_receipt_balance)
        cash_balance = 0
        bonuses_balance = 0
        electronical_balance = 0
        sbp_receipt_balance = 0
        is_waiting_receipt = false

        if visible_session ~= "" then 
            hardware:CloseVisibleSession();
            hardware:EndSession();
        end
        hardware:CreateSession();
        is_at_start = true
        return mode_choose_method
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

show_sbp_select_price = function(balance_rur)
    balance_int = math.ceil(balance_rur)
    sbp_select_price:Set("sbp_balance.value", balance_int)
    sbp_select_price:Display()
end

show_wait_for_QR = function(seconds_float)
    seconds_int = math.ceil(seconds_float)
    wait_for_QR:Set("delay_seconds.value", seconds_int)
    wait_for_QR:Display()
end

show_sorry = function()
    sorry:Display()
end

show_sbp_payment = function()
    sbp_payment:Set("qr_sbp.url", sbpQr)
    sbp_payment:Display()
end

show_choose_method = function()
    if hascardreader() then
        choose_method:Set("card_pic.visible", "true")
        choose_method:Set("card.visible", "true")
    else
        choose_method:Set("card_pic.visible", "false")
        choose_method:Set("card.visible", "false")
    end
    if is_connected_to_sbp then
        choose_method:Set("sbp_pic.visible", "true")
        choose_method:Set("sbp.visible", "true")
    else
        choose_method:Set("sbp_pic.visible", "false")
        choose_method:Set("sbp.visible", "false")
    end

    choose_method:Set("post_number.value", post_position)
    choose_method:Set("qr_pic.url", qr)
    choose_method:Display()
end

show_connected_to_bonus_system = function()
    connected_to_bonus_system:Display()
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
    working:Set("p6_ico.visible", "true")
    working:Set("p6_ico_stop.visible", "false")
    switch_submodes(sub_mode)
    working:Display()
end

show_pause = function(balance_rur, balance_sec, price_rur)
    balance_int = math.ceil(balance_rur)
    sec_int = math.ceil(balance_sec)
    working:Set("pause_digits.visible", "true")
    working:Set("pause_digits.value", sec_int)
    working:Set("balance.value", balance_int)
    working:Set("price.value", price_rur)

    if is_authorized_function() then
        working:Set("p6_ico.visible", "false")
        working:Set("p6_ico_stop.visible", "true")
    end

    switch_submodes(6)
    working:Display()
end

switch_submodes = function(sub_mode)
    working:Set("cur_p.index", sub_mode-1)  
end

show_confirm_end = function()
    confirm_end:Display()
end

show_thanks = function(seconds_float)
    seconds_int = math.ceil(seconds_float)
    thanks:Set("delay_seconds.value", seconds_int)
    thanks:Display()
end

get_mode_by_pressed_key = function()
    pressed_key = get_key()
    if pressed_key >= 1 and pressed_key<=5 then return mode_work + pressed_key end
    if pressed_key == 6 then return mode_pause end
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

send_receipt = function(post_pos, cash, electronical, sbp_receipt_balance)
    hardware:SendReceipt(post_pos, cash, electronical, sbp_receipt_balance)
end

increment_cars = function()
    hardware:IncrementCars()
end

run_pause = function()
    run_program(6)
end

run_stop = function()
    run_program(-1)
end

run_program = function(program_num)
    if program_num ~= 6 and program_num >0 then
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

get_is_sbp_payment_on_terminal_available = function()
    return hardware:GetIsSbpPaymentOnTerminalAvailable()
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
    new_bonuses = hardware:GetBonuses()
    new_sbp = hardware:GetSbpMoney()

    if new_coins > 0 or new_banknotes > 0 or new_electronical > 0 or new_service > 0 or new_bonuses > 0 then
        is_money_added = true
        money_wait_seconds = max_money_wait_seconds * 10
    end

    cash_balance = cash_balance + new_coins
    cash_balance = cash_balance + new_banknotes
    electronical_balance = electronical_balance + new_electronical
    sbp_receipt_balance = sbp_receipt_balance + new_sbp
    bonuses_balance = bonuses_balance + new_bonuses
    balance = balance + new_coins
    balance = balance + new_banknotes
    balance = balance + new_electronical
    balance = balance + new_service
    balance = balance + new_bonuses
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

is_authorized_function = function ()
    authorizedSessionID = hardware:AuthorizedSessionID();
    if authorizedSessionID ~= "" and authorizedSessionID ~= nil then
        return true
    end
    return false
end

create_session = function()
    hardware:CreateSession();
    get_QR()
end

get_is_connected_to_bonus_system = function()
    return hardware:GetIsConnectedToBonusSystem();
end

get_is_connected_to_sbp_system = function()
    return hardware:SbpSystemIsActive();
end

set_is_connected_to_bonus_system = function(connectedToBonusSystem)
    hardware:SetIsConnectedToBonusSystem(connectedToBonusSystem);
end

get_QR = function()
    qr = hardware:GetQR();
    if qr == nil or qr == '' or hardware:BonusSystemIsActive() == false then
        choose_method:Set("qr_pic.visible", "false")
        choose_method:Set("bonus_pic.visible", "false")
    else
        choose_method:Set("qr_pic.visible", "true")
        choose_method:Set("bonus_pic.visible", "true")
    end
end

get_sbp_qr = function()
    sbpQr = hardware:GetSbpQR();
    if sbpQr == nil or sbpQr == '' then
        sbp_payment:Set("qr_sbp.visible", "false")
    else
        sbp_payment:Set("qr_sbp.visible", "true")
    end
end

no_internet_mode = function()
    show_no_internet()
    run_stop()
    turn_light(0, animation.idle)
    
    if hardware:GetBlockState() ~= BlockState.NO_SERVER_CONNECTION then
        return mode_welcome
    end
    
    return mode_no_internet
end

show_no_internet = function()
    no_internet_connection:Display()
end