-- Vacuum Firmware

-- setup is running at the start just once
setup = function()
    -- global variables
    balance_left = 0.0
    balance_right = 0.0
    electron_balance_left = 0
    electron_balance_right = 0
    cash_balance_left = 0
    cash_balance_right = 0
    electronical_balance_left = 0
    electronical_balance_right = 0

    waiting_loops_left = 0
    waiting_loops_right = 0

    min_electron_balance = 10
    max_electron_balance = 900
    max_cash_balance = 900

    keyboard_pressed_left = false
    keyboard_pressed_right = false

    fundraiser_active_left = false      -- States that payment is currently in progress for the left/right screen. Needed to get the money where it needs
    fundraiser_active_right = false

    left_thanks_mode_current = false
    right_thanks_mode_current = false

    starting_balance_right = 0
    starting_balance_left = 0

    -- constants
    post_position = 1
    welcome_mode_seconds = 3
    thanks_mode_seconds = 5
    apology_mode_seconds = 5
    wait_mode_seconds = 40

    is_transaction_started = false
    is_waiting_receipt_left = false
    is_waiting_receipt_right = false

    -- buttons
    cash_button_left = 100
    cashless_button_left = 101
    begin_button_left = 102
    cancel_button_left = 103
    pause_button_left = 104

    cash_button_right = 200
    cashless_button_right = 201
    begin_button_right = 202
    cancel_button_right = 203
    pause_button_right = 204

    price_p = {}
    price_p[0] = 0
    price_p[1] = 0

    price_pause = 0
    price_work = 1

    mode_welcome = 0
    mode_choose = 10
    mode_fundraising = 20
    mode_keyboard = 30
    mode_wait = 40
    mode_work = 50
    mode_thanks = 60
    mode_apology = 70

    real_ms_per_loop = 100
    pressed_key = 0

    current_right_mode = mode_welcome
    current_left_mode = mode_welcome

    actual_program_stop = 0
    actual_program_left_work_right_pause = 1
    actual_program_left_pause_right_work = 2
    actual_program_left_work_right_work = 3
    actual_program_left_pause_right_pause = 4

    program_stop = 0
    program_left_pause = 1
    program_right_pause = 2
    program_left_work = 3
    program_right_work = 4

    current_right_program = program_stop
    current_left_program = program_stop

    init_prices()
    printMessage("Prices: " .. price_p[0] .. " " .. price_p[1] .. " " .. price_p[2] .. " " .. price_p[3] .. " " .. price_p[4])

    version = "1.0.0"

    printMessage("dia generic wash firmware v." .. version)
    smart_delay(1)

    hours = 0
    minutes = 0

    return 0
end

loop = function()
    pressed_key = get_key()
    set_time(get_time_hours(), get_time_minutes())
    set_weather(get_weather_negative(), get_weather_degrees(), get_weather_fraction())
    printMessage(string.format("current state: %2d %1d   %2d %1d", current_left_mode, current_left_program, current_right_mode, current_right_program))
    current_left_mode = left_display_mode(current_left_mode)
    current_right_mode = right_display_mode(current_right_mode)
    real_ms_per_loop = smart_delay(100)
    return 0
end

init_prices = function()
    price_p[program_left_pause] = get_price(actual_program_left_pause_right_pause)
    price_p[program_right_pause] = get_price(actual_program_left_pause_right_pause)
    price_p[program_left_work] = get_price(actual_program_left_work_right_work)
    price_p[program_right_work] = get_price(actual_program_left_work_right_work)
end

-- Mode

right_display_mode = function(new_mode)
    if new_mode == mode_welcome then return right_welcome_mode() end
    if new_mode == mode_choose then return right_choose_mode() end
    if new_mode == mode_fundraising then return right_fundraising_mode() end
    if new_mode == mode_keyboard then return right_keyboard_mode() end
    if new_mode == mode_wait then return right_wait_mode() end
    if new_mode == mode_work then return right_work_mode() end
    if new_mode == mode_thanks then return right_thanks_mode() end
    if new_mode == mode_apology then return right_apology_mode() end
end

left_display_mode = function(new_mode)
    if new_mode == mode_welcome then return left_welcome_mode() end
    if new_mode == mode_choose then return left_choose_mode() end
    if new_mode == mode_fundraising then return left_fundraising_mode() end
    if new_mode == mode_keyboard then return left_keyboard_mode() end
    if new_mode == mode_wait then return left_wait_mode() end
    if new_mode == mode_work then return left_work_mode() end
    if new_mode == mode_thanks then return left_thanks_mode() end
    if new_mode == mode_apology then return left_apology_mode() end
end

right_welcome_mode = function()
    right_show_welcome()
    run_program(current_left_program, current_right_program)
    turn_light(0, animation.idle)
    check_open_lid()
    smart_delay(1000 * welcome_mode_seconds)
    return mode_choose
end

left_welcome_mode = function()
    left_show_welcome()
    run_program(current_left_program, current_right_program)
    turn_light(0, animation.idle)
    check_open_lid()
    smart_delay(1000 * welcome_mode_seconds)
    return mode_choose
end

right_choose_mode = function()
    right_show_choose(balance_right)
    run_program(current_left_program, current_right_program)
    turn_light(0, animation.idle)
    check_open_lid()

    if pressed_key == cash_button_right and fundraiser_active_left == false then
        fundraiser_active_right = true
        return mode_fundraising
    end
    if pressed_key == cashless_button_right then return mode_keyboard end

    return mode_choose
end

left_choose_mode = function()
    left_show_choose(balance_left)
    run_program(current_left_program, current_right_program)
    turn_light(0, animation.idle)
    check_open_lid()

    if pressed_key == cash_button_left and fundraiser_active_right == false then
        fundraiser_active_left = true
        return mode_fundraising
    end
    if pressed_key == cashless_button_left then return mode_keyboard end

    return mode_choose
end

right_fundraising_mode = function()
    right_show_fundraising(balance_right)
    run_program(current_left_program, current_right_program)
    turn_light(0, animation.idle)
    check_open_lid()

    update_balance()
    set_current_state(balance_right + balance_left)

    if pressed_key == begin_button_right then
        fundraiser_active_right = false
        return mode_work
    end
    if pressed_key == cancel_button_right then
        fundraiser_active_right = false
        return mode_choose
    end

    return mode_fundraising
end

left_fundraising_mode = function()
    left_show_fundraising(balance_left)
    run_program(current_left_program, current_right_program)
    turn_light(0, animation.idle)
    check_open_lid()

    update_balance()
    set_current_state(balance_right + balance_left)

    if pressed_key == begin_button_left then
        fundraiser_active_left = false
        return mode_work
    end
    if pressed_key == cancel_button_left then
        fundraiser_active_left = false
        return mode_choose
    end

    return mode_fundraising
end

right_keyboard_mode = function()
    if keyboard_pressed_right then right_show_keyboard(electron_balance_right)
    else right_show_keyboard(min_electron_balance) end

    run_program(current_left_program, current_right_program)
    turn_light(0, animation.idle)
    check_open_lid()

    if pressed_key == cancel_button_right then
        keyboard_pressed_right = false
        electron_balance_right = 0
        fundraiser_active_right = false
        return mode_choose
    end
    if pressed_key == begin_button_right and fundraiser_active_left == false then
        fundraiser_active_right = true
        if keyboard_pressed_right == false then electron_balance_right = min_electron_balance end
        keyboard_pressed_right = false
        return mode_wait
    end
    if pressed_key >= 11 and pressed_key <= 20 then
        keyboard_pressed_right = true
        electron_balance_right = electron_balance_right * 10 + pressed_key - 11
        if electron_balance_right > max_electron_balance then electron_balance_right = max_electron_balance end
    end

    return mode_keyboard
end

left_keyboard_mode = function()
    if keyboard_pressed_left then left_show_keyboard(electron_balance_left)
    else left_show_keyboard(min_electron_balance) end

    run_program(current_left_program, current_right_program)
    turn_light(0, animation.idle)
    check_open_lid()

    if pressed_key == begin_button_left and fundraiser_active_right == false then
        fundraiser_active_left = true
        if keyboard_pressed_left == false then electron_balance_left = min_electron_balance end
        keyboard_pressed_left = false
        return mode_wait
    end
    if pressed_key == cancel_button_left then
        keyboard_pressed_left = false
        electron_balance_left = 0
        fundraiser_active_left = false
        return mode_choose
    end
    if pressed_key >= 1 and pressed_key <= 10 then
        keyboard_pressed_left = true
        electron_balance_left = electron_balance_left * 10 + pressed_key - 1
        if electron_balance_left > max_electron_balance then electron_balance_left = max_electron_balance end
    end

    return mode_keyboard
end

right_wait_mode = function()
    right_show_wait(electron_balance_right)
    run_program(current_left_program, current_right_program)
    turn_light(0, animation.idle)
    check_open_lid()

    if is_transaction_started == false then
        waiting_loops_right = wait_mode_seconds * 10;

        request_transaction(electron_balance_right)
        electron_balance_right = 0
        is_transaction_started = true
    end

    status = get_transaction_status()
    update_balance()

    if balance_right > 0.99 then
        is_transaction_started = false
        fundraiser_active_right = false
        return mode_work
    end

    if waiting_loops_right <= 0 then
        is_transaction_started = false
	    if status ~= 0 then
	        abort_transaction()
	    end
        fundraiser_active_right = false
        return mode_choose
    end

    if status == 0 and is_transaction_started == true then
        is_transaction_started = false
        abort_transaction()
        fundraiser_active_right = false
        return mode_choose
	end

    if pressed_key == cancel_button_right then
        if electron_balance_right == min_electron_balance then 
            keyboard_pressed_right = false
            electron_balance_right = 0
        end
        waiting_loops_right = 0
    end

    smart_delay(100)
    waiting_loops_right = waiting_loops_right - 1
    return mode_wait
end

left_wait_mode = function()
    left_show_wait(electron_balance_left)
    run_program(current_left_program, current_right_program)
    turn_light(0, animation.idle)
    check_open_lid()

    if is_transaction_started == false then
        waiting_loops_left = wait_mode_seconds * 10;

        request_transaction(electron_balance_left)
        electron_balance_left = 0
        is_transaction_started = true
    end

    status = get_transaction_status()
    update_balance()

    if balance_left > 0.99 then
        if status ~= 0 then
            abort_transaction()
        end
        is_transaction_started = false
        fundraiser_active_left = false
        return mode_work
    end

    if waiting_loops_left <= 0 then
        is_transaction_started = false
	    if status ~= 0 then
	        abort_transaction()
	    end
        fundraiser_active_left = false
        return mode_choose
    end

    if status == 0 and is_transaction_started == true then
        is_transaction_started = false
        abort_transaction()
        fundraiser_active_left = false
        return mode_choose
	end

    if pressed_key == cancel_button_left then
        if electron_balance_left == min_electron_balance then 
            keyboard_pressed_left = false
            electron_balance_left = 0
        end
        waiting_loops_left = 0
    end

    smart_delay(100)
    waiting_loops_left = waiting_loops_left - 1
    return mode_wait
end

right_work_mode = function()
    if current_right_program == program_stop then
        starting_balance_right = balance_right
        current_right_program = program_right_work
    end

    right_show_work(balance_right, balance_right / starting_balance_right)

    set_current_state(balance_right + balance_left)

    charge_balance_right(price_p[current_right_program])
    run_program(current_left_program, current_right_program)
    turn_light(0, animation.idle)
    check_open_lid()

    if balance_right <= 0.01 then
        current_right_program = program_stop
        return mode_thanks
    end

    if pressed_key == pause_button_right then
        if current_right_program == program_right_work then current_right_program = program_right_pause
        elseif current_right_program == program_right_pause then current_right_program = program_right_work end
    end

    return mode_work
end

left_work_mode = function()
    if current_left_program == program_stop then
        starting_balance_left = balance_left
        current_left_program = program_left_work
    end

    left_show_work(balance_left, balance_left / starting_balance_left)

    set_current_state(balance_right + balance_left)

    charge_balance_left(price_p[current_left_program])
    run_program(current_left_program, current_right_program)
    turn_light(0, animation.idle)
    check_open_lid()

    if balance_left <= 0.01 then
        current_left_program = program_stop
        return mode_thanks
    end

    if pressed_key == pause_button_left then
        if current_left_program == program_left_work then current_left_program = program_left_pause
        elseif current_left_program == program_left_pause then current_left_program = program_left_work end
    end

    return mode_work
end

right_thanks_mode = function()
    right_show_thanks()
    run_program(current_left_program, current_right_program)
    check_open_lid()

    if right_thanks_mode_current == false then
        right_thanks_mode_current = true

        turn_light(1, animation.one_button)

        balance_right = 0
        set_current_state(balance_left + balance_right)

        send_receipt(post_position, cash_balance_right, electronical_balance_right)
        cash_balance_right = 0
        electronical_balance_right = 0

        waiting_loops_right = thanks_mode_seconds * 10
    end

    if waiting_loops_right <= 0 then
        right_thanks_mode_current = false
        return mode_choose
    end

    if pressed_key == 301 then waiting_loops_right = 0 end
    waiting_loops_right = waiting_loops_right - 1

    return mode_thanks
end

left_thanks_mode = function()
    left_show_thanks()
    run_program(current_left_program, current_right_program)
    check_open_lid()

    if left_thanks_mode_current == false then
        left_thanks_mode_current = true

        turn_light(1, animation.one_button)

        balance_left = 0
        set_current_state(balance_left + balance_right)

        send_receipt(post_position, cash_balance_left, electronical_balance_left)
        cash_balance_left = 0
        electronical_balance_left = 0

        waiting_loops_left = thanks_mode_seconds * 10
    end

    if waiting_loops_left <= 0 then
        left_thanks_mode_current = false
        return mode_choose
    end

    if pressed_key == 300 then waiting_loops_left = 0 end
    waiting_loops_left = waiting_loops_left - 1

    return mode_thanks
end

right_apology_mode = function()
    right_show_apology()
    run_program(current_left_program, current_right_program)
    check_open_lid()

    if waiting_loops_right <= 0 then waiting_loops_right = apology_mode_seconds * 10 end

    waiting_loops_right = waiting_loops_right - 1

    if waiting_loops_right <= 0 then
        return mode_choose
    end

    return mode_apology
end

left_apology_mode = function()
    left_show_apology()
    run_program(current_left_program, current_right_program)
    check_open_lid()

    if waiting_loops_left <= 0 then waiting_loops_left = apology_mode_seconds * 10 end

    waiting_loops_left = waiting_loops_left - 1

    if waiting_loops_left <= 0 then
        return mode_choose
    end

    return mode_apology
end

-- Show

left_show_welcome = function()
    left_disable_visibility()
    main_screen:Display()
end

left_show_choose = function(balance_rur)
    left_disable_visibility()
    left_charge_balance(balance_rur)

    main_screen:Set("price_work_left.visible", "true")
    main_screen:Set("price_pause_left.visible", "true")
    main_screen:Set("price_work_left.value", math.ceil(price_p[program_left_work]))
    main_screen:Set("price_pause_left.value", math.ceil(price_p[program_left_pause]))

    main_screen:Set("programs_left.position", "700;544")
    main_screen:Set("payment_method_left.position", "1300;590")

    -- We are removing the ability to pay in cash
    -- if fundraiser_active_right == false then
    --     main_screen:Set("button_cash_left.position", "1500;587")
    -- end

    if  hascardreader() then main_screen:Set("button_cashless_left.position", "1360;587") end
    main_screen:Display()
end

left_show_fundraising = function(balance_rur)
    left_disable_visibility()
    left_charge_balance(balance_rur)

    main_screen:Set("fundraising_left.position", "700;580")
    if balance_left > 0 then main_screen:Set("button_begin_left.position", "1360;587") end
    main_screen:Set("button_cancel_left.position", "1500;587")

    main_screen:Display()
end

left_show_keyboard = function(balance_rur)
    left_disable_visibility()
    left_charge_balance(balance_rur)

    main_screen:Set("balance_left.position", "30;100")
    main_screen:Set("enter_amount_left.position", "700;678")

    main_screen:Set("button_1_left.position", "780;893")
    main_screen:Set("button_2_left.position", "780;760")
    main_screen:Set("button_3_left.position", "780;627")
    main_screen:Set("button_4_left.position", "913;893")
    main_screen:Set("button_5_left.position", "913;760")
    main_screen:Set("button_6_left.position", "913;627")
    main_screen:Set("button_7_left.position", "1046;893")
    main_screen:Set("button_8_left.position", "1046;760")
    main_screen:Set("button_9_left.position", "1046;627")
    main_screen:Set("button_0_left.position", "1179;760")

    if fundraiser_active_right == false and
        ((electron_balance_left >= min_electron_balance and keyboard_pressed_left == true) or keyboard_pressed_left == false) then
        main_screen:Set("button_begin_left.position", "1360;587")
    end
    main_screen:Set("button_cancel_left.position", "1500;587")

    main_screen:Display()
end

left_show_wait = function(balance_rur)
    left_disable_visibility()
    left_charge_balance(balance_rur)

    main_screen:Set("wait_left.position", "700;597")
    main_screen:Set("button_cancel_left.position", "1500;587")


    main_screen:Display()
end

left_show_work = function(balance_rur, progress_rur)
    left_disable_visibility()
    left_charge_balance(balance_rur)
    left_set_progressbar(progress_rur)

    main_screen:Set("console_left.position", "700;635")
    main_screen:Set("button_pause_left.position", "1500;587")

    main_screen:Display()
end

left_show_thanks = function()
    left_disable_visibility()
    left_charge_balance(0)
    main_screen:Set("thanks_left.position", "700;580")
    main_screen:Set("background_left.click_id", "300")
    main_screen:Display()
end

left_show_apology = function()
    left_disable_visibility()
    left_charge_balance(0)
    main_screen:Set("apology_left.position", "700;586")
    main_screen:Display()
end

right_show_welcome = function()
    right_disable_visibility()
    main_screen:Display()
end

right_show_choose = function(balance_rur)
    right_disable_visibility()
    right_charge_balance(balance_rur)

    main_screen:Set("price_work_right.visible", "true")
    main_screen:Set("price_pause_right.visible", "true")
    main_screen:Set("price_work_right.value", math.ceil(price_p[program_right_work]))
    main_screen:Set("price_pause_right.value", math.ceil(price_p[program_right_pause]))

    main_screen:Set("programs_right.position", "700;8")
    main_screen:Set("payment_method_right.position", "1300;101")

    -- We are removing the ability to pay in cash
    -- if fundraiser_active_left == false then
    --     main_screen:Set("button_cash_right.position", "1500;77")
    -- end

    if hascardreader() then main_screen:Set("button_cashless_right.position", "1360;77") end
    main_screen:Display()
end

right_show_fundraising = function(balance_rur)
    right_disable_visibility()
    right_charge_balance(balance_rur)

    main_screen:Set("fundraising_right.position", "700;79")
    if balance_right > 0 then main_screen:Set("button_begin_right.position", "1360;77") end
    main_screen:Set("button_cancel_right.position", "1500;77")

    main_screen:Display()
end

right_show_keyboard = function(balance_rur)
    right_disable_visibility()
    right_charge_balance(balance_rur)

    main_screen:Set("balance_right.position", "300;100")
    main_screen:Set("enter_amount_right.position", "700;156")

    main_screen:Set("button_1_right.position", "780;353")
    main_screen:Set("button_2_right.position", "780;220")
    main_screen:Set("button_3_right.position", "780;87")
    main_screen:Set("button_4_right.position", "913;353")
    main_screen:Set("button_5_right.position", "913;220")
    main_screen:Set("button_6_right.position", "913;87")
    main_screen:Set("button_7_right.position", "1046;353")
    main_screen:Set("button_8_right.position", "1046;220")
    main_screen:Set("button_9_right.position", "1046;87")
    main_screen:Set("button_0_right.position", "1179;220")

    if fundraiser_active_left == false and 
        ((electron_balance_right >= min_electron_balance and keyboard_pressed_right == true) or keyboard_pressed_right == false) then
        main_screen:Set("button_begin_right.position", "1360;77")
    end
    main_screen:Set("button_cancel_right.position", "1500;77")

    main_screen:Display()
end

right_show_wait = function(balance_rur)
    right_disable_visibility()
    right_charge_balance(balance_rur)

    main_screen:Set("wait_right.position", "700;57")
    main_screen:Set("button_cancel_right.position", "1500;77")

    main_screen:Display()
end

right_show_work = function(balance_rur, progress_rur)
    right_disable_visibility()
    right_charge_balance(balance_rur)
    right_set_progressbar(progress_rur)

    main_screen:Set("console_right.position", "700;102")
    main_screen:Set("button_pause_right.position", "1500;77")

    main_screen:Display()
end

right_show_thanks = function()
    right_disable_visibility()
    right_charge_balance(0)
    main_screen:Set("thanks_right.position", "700;79")
    main_screen:Set("background_right.click_id", "301")
    main_screen:Display()
end

right_show_apology = function()
    right_disable_visibility()
    right_charge_balance(0)
    main_screen:Set("apology_right.position", "700;92")
    main_screen:Display()
end

right_disable_visibility = function()
    main_screen:Set("background_right.click_id", "0")
    main_screen:Set("price_work_right.visible", "false")
    main_screen:Set("price_pause_right.visible", "false")
    main_screen:Set("programs_right.position", "2000;2000")
    main_screen:Set("payment_method_right.position", "2000;2000")
    main_screen:Set("button_cash_right.position", "2000;2000")
    main_screen:Set("button_cashless_right.position", "2000;2000")
    main_screen:Set("fundraising_right.position", "2000;2000")
    main_screen:Set("button_begin_right.position", "2000;2000")
    main_screen:Set("button_cancel_right.position", "2000;2000")
    main_screen:Set("button_pause_right.position", "2000;2000")
    main_screen:Set("enter_amount_right.position", "2000;2000")
    main_screen:Set("button_0_right.position", "2000;2000")
    main_screen:Set("button_1_right.position", "2000;2000")
    main_screen:Set("button_2_right.position", "2000;2000")
    main_screen:Set("button_3_right.position", "2000;2000")
    main_screen:Set("button_4_right.position", "2000;2000")
    main_screen:Set("button_5_right.position", "2000;2000")
    main_screen:Set("button_6_right.position", "2000;2000")
    main_screen:Set("button_7_right.position", "2000;2000")
    main_screen:Set("button_8_right.position", "2000;2000")
    main_screen:Set("button_9_right.position", "2000;2000")
    main_screen:Set("wait_right.position", "2000;2000")
    main_screen:Set("console_right.position", "2000;2000")
    main_screen:Set("thanks_right.position", "2000;2000")
    main_screen:Set("apology_right.position", "2000;2000")
    main_screen:Set("progressbar_1_right.position",  "2000;2000")
    main_screen:Set("progressbar_2_right.position",  "2000;2000")
    main_screen:Set("progressbar_3_right.position",  "2000;2000")
    main_screen:Set("progressbar_4_right.position",  "2000;2000")
    main_screen:Set("progressbar_5_right.position",  "2000;2000")
    main_screen:Set("progressbar_6_right.position",  "2000;2000")
    main_screen:Set("progressbar_7_right.position",  "2000;2000")
    main_screen:Set("progressbar_8_right.position",  "2000;2000")
    main_screen:Set("progressbar_9_right.position",  "2000;2000")
    main_screen:Set("progressbar_10_right.position", "2000;2000")
    main_screen:Set("progressbar_11_right.position", "2000;2000")
    main_screen:Set("progressbar_12_right.position", "2000;2000")
    main_screen:Set("progressbar_13_right.position", "2000;2000")
    main_screen:Set("progressbar_14_right.position", "2000;2000")
end

left_disable_visibility = function()
    main_screen:Set("background_left.click_id", "0")
    main_screen:Set("price_work_left.visible", "false")
    main_screen:Set("price_pause_left.visible", "false")
    main_screen:Set("programs_left.position", "2000;2000")
    main_screen:Set("payment_method_left.position", "2000;2000")
    main_screen:Set("button_cash_left.position", "2000;2000")
    main_screen:Set("button_cashless_left.position", "2000;2000")
    main_screen:Set("fundraising_left.position", "2000;2000")
    main_screen:Set("button_begin_left.position", "2000;2000")
    main_screen:Set("button_cancel_left.position", "2000;2000")
    main_screen:Set("button_pause_left.position", "2000;2000")
    main_screen:Set("enter_amount_left.position", "2000;2000")
    main_screen:Set("button_0_left.position", "2000;2000")
    main_screen:Set("button_1_left.position", "2000;2000")
    main_screen:Set("button_2_left.position", "2000;2000")
    main_screen:Set("button_3_left.position", "2000;2000")
    main_screen:Set("button_4_left.position", "2000;2000")
    main_screen:Set("button_5_left.position", "2000;2000")
    main_screen:Set("button_6_left.position", "2000;2000")
    main_screen:Set("button_7_left.position", "2000;2000")
    main_screen:Set("button_8_left.position", "2000;2000")
    main_screen:Set("button_9_left.position", "2000;2000")
    main_screen:Set("wait_left.position", "2000;2000")
    main_screen:Set("console_left.position", "2000;2000")
    main_screen:Set("thanks_left.position", "2000;2000")
    main_screen:Set("apology_left.position", "2000;2000")
    main_screen:Set("progressbar_1_left.position",  "2000;2000")
    main_screen:Set("progressbar_2_left.position",  "2000;2000")
    main_screen:Set("progressbar_3_left.position",  "2000;2000")
    main_screen:Set("progressbar_4_left.position",  "2000;2000")
    main_screen:Set("progressbar_5_left.position",  "2000;2000")
    main_screen:Set("progressbar_6_left.position",  "2000;2000")
    main_screen:Set("progressbar_7_left.position",  "2000;2000")
    main_screen:Set("progressbar_8_left.position",  "2000;2000")
    main_screen:Set("progressbar_9_left.position",  "2000;2000")
    main_screen:Set("progressbar_10_left.position", "2000;2000")
    main_screen:Set("progressbar_11_left.position", "2000;2000")
    main_screen:Set("progressbar_12_left.position", "2000;2000")
    main_screen:Set("progressbar_13_left.position", "2000;2000")
    main_screen:Set("progressbar_14_left.position", "2000;2000")
end

right_charge_balance = function(balance_rur)
    balance_int = math.ceil(balance_rur)
    main_screen:Set("balance_right.value", balance_int)
end

left_charge_balance = function(balance_rur)
    balance_int = math.ceil(balance_rur)
    main_screen:Set("balance_left.value", balance_int)
end

right_set_progressbar = function(progress_rur)
    progress_int = math.ceil(progress_rur * 14)
    if progress_int >= 1  then main_screen:Set("progressbar_1_right.position",  "570;425") end
    if progress_int >= 2  then main_screen:Set("progressbar_2_right.position",  "591;420") end
    if progress_int >= 3  then main_screen:Set("progressbar_3_right.position",  "603;406") end
    if progress_int >= 4  then main_screen:Set("progressbar_4_right.position",  "613;382") end
    if progress_int >= 5  then main_screen:Set("progressbar_5_right.position",  "621;348") end
    if progress_int >= 6  then main_screen:Set("progressbar_6_right.position",  "627;307") end
    if progress_int >= 7  then main_screen:Set("progressbar_7_right.position",  "629;261") end

    if progress_int >= 8  then main_screen:Set("progressbar_8_right.position",  "629;209") end
    if progress_int >= 9  then main_screen:Set("progressbar_9_right.position",  "626;163") end
    if progress_int >= 10 then main_screen:Set("progressbar_10_right.position", "621;123") end
    if progress_int >= 11 then main_screen:Set("progressbar_11_right.position", "613;89") end
    if progress_int >= 12 then main_screen:Set("progressbar_12_right.position", "603;64") end
    if progress_int >= 13 then main_screen:Set("progressbar_13_right.position", "591;50") end
    if progress_int >= 14 then main_screen:Set("progressbar_14_right.position", "570;45") end
end

left_set_progressbar = function(progress_rur)
    progress_int = math.ceil(progress_rur * 14)
    if progress_int >= 1  then main_screen:Set("progressbar_1_left.position",  "570;965") end
    if progress_int >= 2  then main_screen:Set("progressbar_2_left.position",  "591;960") end
    if progress_int >= 3  then main_screen:Set("progressbar_3_left.position",  "603;946") end
    if progress_int >= 4  then main_screen:Set("progressbar_4_left.position",  "613;922") end
    if progress_int >= 5  then main_screen:Set("progressbar_5_left.position",  "621;888") end
    if progress_int >= 6  then main_screen:Set("progressbar_6_left.position",  "627;847") end
    if progress_int >= 7  then main_screen:Set("progressbar_7_left.position",  "629;801") end
    if progress_int >= 8  then main_screen:Set("progressbar_8_left.position",  "629;749") end
    if progress_int >= 9  then main_screen:Set("progressbar_9_left.position",  "626;703") end
    if progress_int >= 10 then main_screen:Set("progressbar_10_left.position", "621;663") end
    if progress_int >= 11 then main_screen:Set("progressbar_11_left.position", "613;629") end
    if progress_int >= 12 then main_screen:Set("progressbar_12_left.position", "603;604") end
    if progress_int >= 13 then main_screen:Set("progressbar_13_left.position", "591;590") end
    if progress_int >= 14 then main_screen:Set("progressbar_14_left.position", "570;585") end
end

set_time = function(hours_rur, minutes_rur)
    main_screen:Set("hours.value",  hours_rur)
    main_screen:Set("minutes.value",  minutes_rur)
    if minutes_rur <= 9 then main_screen:Set("minutes_zero.visible", "true")
    else main_screen:Set("minutes_zero.visible", "false") end
end

set_weather = function(negative_rur, degrees_rur, fraction_rur)
    if negative_rur then
        main_screen:Set("plus.position", "2000;2000")
        if degrees_rur >= 10 then main_screen:Set("minus.position", "165;1005")
        else main_screen:Set("minus.position", "165;990") end
    else
        if degrees_rur >= 10 then main_screen:Set("plus.position", "165;1005")
        else main_screen:Set("plus.position", "165;990") end
        main_screen:Set("minus.position", "2000;2000")
    end
    main_screen:Set("degrees.value",  degrees_rur)
    main_screen:Set("fraction.value",  fraction_rur)
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
    hardware:SendReceipt(post_pos, cash, electronical, 0)
end

increment_cars = function()
    hardware:IncrementCars()
end

run_program = function(program_left_num, program_right_num)
    if program_left_num == program_stop and program_right_num == program_stop then hardware:TurnProgram(actual_program_stop)
    elseif program_left_num == program_stop and program_right_num == program_right_pause then hardware:TurnProgram(actual_program_left_pause_right_pause)
    elseif program_left_num == program_stop and program_right_num == program_right_work then hardware:TurnProgram(actual_program_left_pause_right_work)
    elseif program_left_num == program_left_pause and program_right_num == program_stop then hardware:TurnProgram(actual_program_left_pause_right_pause)
    elseif program_left_num == program_left_work and program_right_num == program_stop then hardware:TurnProgram(actual_program_left_work_right_pause)

    elseif program_left_num == program_left_work and program_right_num == program_right_pause then hardware:TurnProgram(actual_program_left_work_right_pause)
    elseif program_left_num == program_left_pause and program_right_num == program_right_work then hardware:TurnProgram(actual_program_left_pause_right_work)
    elseif program_left_num == program_left_work and program_right_num == program_right_work then hardware:TurnProgram(actual_program_left_work_right_work)
    elseif program_left_num == program_left_pause and program_right_num == program_right_pause then hardware:TurnProgram(actual_program_left_pause_right_pause) end
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

update_balance = function()
    new_coins = hardware:GetCoins()
    new_banknotes = hardware:GetBanknotes()
    new_electronical = hardware:GetElectronical()
    new_service = hardware:GetService()

    if fundraiser_active_left then
        cash_balance_left = cash_balance_left + new_coins + new_banknotes
        electronical_balance_left = electronical_balance_left + new_electronical
        balance_left = balance_left + new_coins + new_banknotes + new_electronical + new_service
    elseif fundraiser_active_right then
        cash_balance_right = cash_balance_right + new_coins + new_banknotes
        electronical_balance_right = electronical_balance_right + new_electronical
        balance_right = balance_right + new_coins + new_banknotes + new_electronical + new_service
    end
end

charge_balance_left = function(price)
    balance_left = balance_left - price * real_ms_per_loop / 60000
    if balance_left < 0 then balance_left = 0 end
end

charge_balance_right = function(price)
    balance_right = balance_right - price * real_ms_per_loop / 60000
    if balance_right < 0 then balance_right = 0 end
end

hascardreader = function()
  return hardware:HasCardReader()
end

get_weather_degrees = function()
    return weather:GetTempDegrees()
end

get_weather_fraction = function()
    return weather:GetTempFraction()
end

get_weather_negative = function()
    return weather:IsNegative()
end

get_time_hours = function()
    return hardware:GetHours()
end

get_time_minutes = function()
    return hardware:GetMinutes()
end

need_to_open_lid = function()
    val = hardware:GetOpenLid()
    if val>0 then return true end
    return false
end

check_open_lid = function()
    if need_to_open_lid() then
        printMessage("LID OPENED ...")
        hardware:TurnProgram(11)
        printMessage("LID OPENED :(")
        smart_delay(9500)
        hardware:TurnProgram(actual_program_stop)
        printMessage("LID CLOSED :)")
    end
end
