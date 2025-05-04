-- Robot Firmware

-- setup is running at the start just once
setup = function()
    -- global variables

    balance = 0.0
    start_balance = 0

    electron_balance = 0
    
    cash_balance = 0.0
    electronical_balance = 0.0
    sbp_receipt_balance = 0.0
    bonuses_balance = 0.0
    post_position = 1

    keyboard_pressed = false 

    -- constants
    go_to_box_mode_seconds = 10
    wait_card_mode_seconds = 40
    wait_sbp_qr_seconds = 15
    apology_mode_seconds = 10
    wait_mode_seconds = 40
    inactive_seconds = 120
    
    is_authorized = false
    is_transaction_started = false
    is_waiting_receipt = false
    is_money_added = false
    is_connected_to_sbp = false

    price_p = {}
    
    price_p[0] = 0
    price_p[1] = 0
    price_p[2] = 0
    price_p[3] = 0
    price_p[4] = 0

    discount_p = {}
    
    discount_p[0] = 0
    discount_p[1] = 0
    discount_p[2] = 0
    discount_p[3] = 0
    discount_p[4] = 0

    init_prices()
    init_discounts()
    
    mode_choose_program = 10
    mode_choose_payment = 20
    mode_wait_for_cash = 30
    mode_wait_for_card = 40
    mode_sbp_payment = 50
    mode_wait_for_QR = 51
    mode_sorry = 52
    mode_go_to_box = 70

    real_ms_per_loop = 100
    
    current_mode = mode_choose_program

    waiting_loops_progress = 0
    waiting_loops = 0

    required_payment = 0

    version = "1.0.0"

    create_session()

    qr = ""
    sbpQr = ""
    sbpQrTemp = ""
    visible_session = ""

    selected_program = 0

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
    price_p[2] = get_price(2)
    price_p[3] = get_price(3)
    price_p[4] = get_price(4)
end

init_discounts = function()
    discount_p[1] = get_discount(1)
    discount_p[2] = get_discount(2)
    discount_p[3] = get_discount(3)
    discount_p[4] = get_discount(4)
end

-- Mode

run_mode = function(new_mode)
    if new_mode == mode_choose_program then return choose_program_mode() end
    if new_mode == mode_choose_payment then return choose_payment_mode() end
    if new_mode == mode_wait_for_cash then return wait_for_cash_mode() end
    if new_mode == mode_wait_for_card then return wait_for_card_mode() end
    if new_mode == mode_wait_for_QR then return wait_for_QR_mode() end
    if new_mode == mode_sorry then return sorry_mode() end
    if new_mode == mode_sbp_payment then return sbp_payment_mode() end
    if new_mode == mode_go_to_box then return go_to_box_mode() end
end

choose_program_mode = function()

    run_stop()
    init_prices()
    init_discounts()
    show_choose_program()

    turn_light(0, animation.idle)

    pressed_key = get_key()
    if pressed_key > 0 and pressed_key <= 4 then
        waiting_loops = inactive_seconds * 10
        set_selected_program(pressed_key)
        return mode_choose_payment
    end
    
    return mode_choose_program
end

choose_payment_mode = function()
    visible_session = hardware:GetVisibleSession();
    is_connected_to_sbp = get_is_connected_to_sbp_system()
    qr = get_QR()

    show_choose_payment(qr, is_connected_to_sbp, required_payment, balance)
    run_stop()

    turn_light(0, animation.idle)

    pressed_key = get_key()

    if pressed_key == 1 then
        waiting_loops = inactive_seconds * 10
        return mode_wait_for_cash
    end
    if is_connected_to_sbp and required_payment >= 10 and pressed_key == 2 then
        hardware:CreateSbpPayment(required_payment)
        sbp_balance = required_payment
        waiting_loops = wait_sbp_qr_seconds * 10;
        return mode_wait_for_QR
    end
    if hascardreader() and pressed_key == 3 then
        electron_balance = required_payment
        return mode_wait_for_card
    end
    if pressed_key == 4 then 
        set_selected_program(0)
        return mode_choose_program
    end

    update_balance()
    if required_payment <= 0 then
        return mode_go_to_box
    end

    if balance <= 0 then 
        waiting_loops = waiting_loops - 1
        if waiting_loops <= 0 then
            return mode_choose_program
        end
    end

    return mode_choose_payment
end

wait_for_cash_mode = function()
    show_wait_for_cash()
    run_stop()

    turn_light(0, animation.idle)

    update_balance()
    if required_payment <= 0 then
        return mode_go_to_box
    end

    pressed_key = get_key()
    if pressed_key > 0 and pressed_key <= 4 then
        waiting_loops = inactive_seconds * 10
        return mode_choose_payment
    end

    if cash_balance <= 0 then 
        waiting_loops = waiting_loops - 1
        if waiting_loops <= 0 then
            waiting_loops = inactive_seconds * 10
            return mode_choose_payment
        end
    end

    return mode_wait_for_cash
end

wait_for_card_mode = function()
    show_wait_for_card()
    run_stop()

    turn_light(0, animation.idle)

    if is_transaction_started == false then
        waiting_loops = wait_card_mode_seconds * 10;

        request_transaction(electron_balance)
        electron_balance = 0
        is_transaction_started = true
    end

    pressed_key = get_key()
    if pressed_key > 0 and pressed_key <= 4 then
        waiting_loops = 0
    end

    status = get_transaction_status()
    update_balance()

    if required_payment <= 0 then
        if status ~= 0 then
            abort_transaction()
        end
        is_transaction_started = false
        return mode_go_to_box
    end

    if waiting_loops <= 0 then
        is_transaction_started = false
	    if status ~= 0 then
	        abort_transaction()
	    end
        waiting_loops = inactive_seconds * 10
        return mode_choose_payment
    end

    if (status == 0) and (is_transaction_started == true) then
        is_transaction_started = false
        abort_transaction()
        waiting_loops = inactive_seconds * 10
        return mode_choose_payment        
	end

    smart_delay(100)
    waiting_loops = waiting_loops - 1
   
    return mode_wait_for_card
end

wait_for_QR_mode = function()
    run_stop()

    turn_light(0, animation.idle)

    sbpQr = get_sbp_qr()
    pressed_key = get_key()
    if sbpQr == '' or sbpQr == nil or sbpQr == sbpQrTemp then
        if waiting_loops > 0 then
            show_wait_for_QR(waiting_loops/10)
            waiting_loops = waiting_loops - 1
        else
            waiting_loops = apology_mode_seconds * 10
            return mode_sorry
        end
    else
        sbpQrTemp = sbpQr
        return mode_sbp_payment
    end

    return mode_wait_for_QR
end

sorry_mode = function()
    run_stop()
    show_sorry()

    turn_light(0, animation.idle)

    pressed_key = get_key()
    if pressed_key > 0 and pressed_key <= 4 then
        waiting_loops = 0
    end
    
    if waiting_loops > 0 then
        waiting_loops = waiting_loops - 1
    else
        waiting_loops = inactive_seconds * 10
        return mode_choose_payment
    end

    return mode_sorry
end

sbp_payment_mode = function()

    turn_light(0, animation.idle)

    sbp_qr = get_sbp_qr()

    if sbpQr == nil or sbpQr == '' then
        waiting_loops = inactive_seconds * 10
        return mode_choose_payment
    end

    show_sbp_payment(sbp_qr)

    run_stop()

    pressed_key = get_key()

    if pressed_key > 0 and pressed_key <= 4 then
        waiting_loops = inactive_seconds * 10
        return mode_choose_payment
    end

    update_balance()
    if required_payment <= 0 then
        return mode_go_to_box
    end

    return mode_sbp_payment
end

go_to_box_mode = function()
    show_go_to_box()

    turn_light(0, animation.idle)

    set_current_state(0)

    if is_waiting_receipt == false then
        balance = 0
        run_program(selected_program)
        waiting_loops = go_to_box_mode_seconds * 10;
        is_waiting_receipt = true
        if visible_session ~= "" then 
            hardware:CloseVisibleSession();
            hardware:EndSession();
        end
    end

    if waiting_loops > 0 then
        if waiting_loops <= 90 then
            run_stop()
            pressed_key = get_key()
            if pressed_key > 0 and pressed_key <= 4 then
                waiting_loops = 0
            end
        end
        waiting_loops = waiting_loops - 1
    else
        send_receipt(post_position, cash_balance, electronical_balance, sbp_receipt_balance)
        increment_cars()
        cash_balance = 0
        bonuses_balance = 0
        electronical_balance = 0
        sbp_receipt_balance = 0
        is_waiting_receipt = false

        hardware:CreateSession();

        set_selected_program(0)
        return mode_choose_program
    end

    return mode_go_to_box
end

-- Show

show_choose_program = function()
    price1_int = math.ceil(price_p[1])
    choose_program:Set("price_p1.value", price1_int)
    price2_int = math.ceil(price_p[2])
    choose_program:Set("price_p2.value", price2_int)
    price3_int = math.ceil(price_p[3])
    choose_program:Set("price_p3.value", price3_int)
    price4_int = math.ceil(price_p[4])
    choose_program:Set("price_p4.value", price4_int)

    choose_program:Display()
end

show_choose_payment = function(qr, is_connected_to_sbp, required_payment, balance)

    if qr == nil or qr == '' or hardware:BonusSystemIsActive() == false or balance > 0 then
        choose_payment:Set("qr_pic.visible", "false")
        choose_payment:Set("bonus_pic.visible", "false")
    else
        choose_payment:Set("qr_pic.visible", "true")
        choose_payment:Set("bonus_pic.visible", "true")
    end

    if is_connected_to_sbp ~= true or required_payment < 10 then
        choose_payment:Set("sbp.visible", "false")
        choose_payment:Set("sbp_arrow.visible", "false")
    else
        choose_payment:Set("sbp.visible", "true")
        choose_payment:Set("sbp_arrow.visible", "true")
    end

    if balance > 0 then
        balance_int = math.ceil(balance)
        choose_payment:Set("balance.visible", "true")
        choose_payment:Set("balance.value", balance_int)
        choose_payment:Set("rub.visible", "true")
    else
        choose_payment:Set("balance.visible", "false")
        choose_payment:Set("rub.visible", "false")
    end

    choose_payment:Set("qr_pic.url", qr)
    choose_payment:Display()
end

show_wait_for_cash = function()
    wait_for_cash:Display()
end

show_wait_for_card = function()
    wait_for_card:Display()
end

show_wait_for_QR = function(seconds_float)
    seconds_int = math.ceil(seconds_float)
    wait_for_QR:Set("delay_seconds.value", seconds_int)
    wait_for_QR:Display()
end

show_sorry = function()
    sorry:Display()
end

show_sbp_payment = function(sbpQr)
    if sbpQr == nil or sbpQr == '' then
        sbp_payment:Set("qr_sbp.visible", "false")
    else
        sbp_payment:Set("qr_sbp.visible", "true")
    end
    sbp_payment:Set("qr_sbp.url", sbpQr)
    sbp_payment:Display()
end

show_go_to_box = function()
    go_to_box:Display()
end

-- Util

set_selected_program = function(program_num)
    selected_program = program_num
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

turn_light = function(rel_num, animation_code)
    hardware:TurnLight(rel_num, animation_code)
end

send_receipt = function(post_pos, cash, electronical, qrMoney)
    hardware:SendReceipt(post_pos, cash, electronical, qrMoney)
end

increment_cars = function()
    hardware:IncrementCars()
end

run_stop = function()
    set_current_state(balance)
    run_program(0)
end

run_program = function(program_num)
    hardware:TurnProgram(program_num)
end

request_transaction = function(balance)
    return hardware:RequestTransaction(balance)
end

confirm_transaction = function(balance)
    return hardware:ConfirmTransaction(math.floor(balance))
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

update_balance = function()
    new_coins = hardware:GetCoins()
    new_banknotes = hardware:GetBanknotes()
    new_electronical = hardware:GetElectronical()
    new_service = hardware:GetService()
    new_bonuses = hardware:GetBonuses()
    new_sbp = hardware:GetSbpMoney()

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

    required_payment = price_p[selected_program] - balance
end

forget_pressed_key = function()
    key = get_key()
end

hascardreader = function()
  return hardware:HasCardReader()
end

create_session = function()
    hardware:CreateSession();
end

get_is_connected_to_sbp_system = function()
    return hardware:SbpSystemIsActive();
end

get_QR = function()
    return hardware:GetQR();
end

get_sbp_qr = function()
    return hardware:GetSbpQR();
end