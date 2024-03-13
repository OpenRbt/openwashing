-- Vacuum Firmware

-- setup is running at the start just once
setup = function()
    -- global variables

    is_paused = false

    can_play_video = false
    is_playing_video = false

    balance = 0.0
    start_balance = 0

    min_electron_balance = 10
    max_electron_balance = 900
    electron_balance = 0
    volume = 0
    tmp_volume = 0
    
    balance_seconds = 0
    cash_balance = 0.0
    electronical_balance = 0.0
    post_position = 1

    keyboard_pressed = false 

    -- constants
    welcome_mode_seconds = 0.1
    thanks_mode_seconds = 5
    apology_mode_seconds = 10
    wait_mode_seconds = 40
    play_video_wait_time = 180
    
    is_transaction_started = false
    is_waiting_receipt = false

    -- buttons
    button_cash = 100
    button_cashless = 101
    button_begin = 102
    button_cancel = 103
    button_pause = 201
    button_play = 202

    price_p = {}
    
    price_p[0] = 0
    price_p[1] = 0

    init_prices()
    
    mode_welcome = 10
    mode_choose = 20
    mode_play_video = 25
    mode_fundraising = 30
    mode_keyboard = 40
    mode_wait = 50
    mode_start_filling = 60
    mode_filling = 70
    mode_thanks = 80
    mode_apology = 90

    real_ms_per_loop = 100
    
    current_mode = mode_welcome

    waiting_loops_progress = 0

    version = "1.0.0"

    printMessage("dia generic wash firmware v." .. version)
    smart_delay(1)
    return 0
end

loop = function()
    printMessage("current_mode: " .. current_mode)
    set_time(get_time_hours(), get_time_minutes())
    set_weather(get_weather_negative(), get_weather_degrees(), get_weather_fraction())
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
    if new_mode == mode_play_video then return play_video_mode() end
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
    check_open_lid()
    turn_light(0, animation.idle)
    smart_delay(1000 * welcome_mode_seconds)
    forget_pressed_key()
    waiting_loops = play_video_wait_time * 10
    return mode_choose
end

choose_mode = function()
    show_choose(price_p[1])
    run_stop()
    check_open_lid()

    turn_light(0, animation.idle)

    pressed_key = get_key()
    if waiting_loops > 0 then
        waiting_loops = waiting_loops - 1
    else
        can_play_video = true
        set_can_play_video(can_play_video)
    end

    is_playing_video = get_is_playing_video()

    if is_playing_video == true then
        return mode_play_video
    end
    
    if pressed_key == button_cash then
        can_play_video = false
        set_can_play_video(can_play_video)
        return mode_fundraising 
    end
    if pressed_key == button_cashless then
        can_play_video = false
        set_can_play_video(can_play_video)
        electron_balance = 0
        return mode_keyboard
    end
    
    update_balance()
    if balance > 0.99 then 
        can_play_video = false
        set_can_play_video(can_play_video)
        return mode_fundraising 
    end

    return mode_choose
end

play_video_mode = function()

    is_playing_video = get_is_playing_video()

    if is_playing_video == false then
        can_play_video = false
        set_can_play_video(can_play_video)
        if get_process_id() == 0 then
            waiting_loops = play_video_wait_time * 10
        else
            waiting_loops = 30 * 10
        end
        return mode_choose
    end

    return mode_play_video
end

keyboard_mode = function()
    if keyboard_pressed then show_keyboard(electron_balance)
    else show_keyboard(min_electron_balance) end
    run_stop()
    check_open_lid()

    turn_light(0, animation.idle)

    pressed_key = get_key()
    if pressed_key >= 1 and pressed_key <= 10 then
        keyboard_pressed = true
        electron_balance = electron_balance * 10 + pressed_key - 1
        if electron_balance > max_electron_balance then electron_balance = max_electron_balance end
    elseif pressed_key == button_cancel then
        keyboard_pressed = false
        electron_balance = 0
        waiting_loops = play_video_wait_time * 10
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
    check_open_lid()

    turn_light(0, animation.idle)

    if is_transaction_started == false then
        waiting_loops = wait_mode_seconds * 10;
        request_transaction_separated(electron_balance)
        print("\n\n\n electron_balance: ", electron_balance)
        is_transaction_started = true
    end

    pressed_key = get_key()
    if pressed_key == button_cancel then waiting_loops = 0 end

    status = get_transaction_status()
    update_balance()

    if balance > 0.99 then
        --if status ~= 0 then abort_transaction() end
        is_transaction_started = false
        return mode_start_filling
    end

    if waiting_loops <= 0 then
        is_transaction_started = false
	    if status ~= 0 then abort_transaction() end
        waiting_loops = play_video_wait_time * 10
        return mode_choose
    end

    if (status == 0) and (is_transaction_started == true) then
        is_transaction_started = false
        abort_transaction()
        waiting_loops = play_video_wait_time * 10
        return mode_choose       
	end

    smart_delay(100)
    waiting_loops = waiting_loops - 1
   
    return mode_wait
end

fundraising_mode = function()
    show_fundraising(balance)
    run_stop()
    check_open_lid()
    turn_light(0, animation.idle)
    init_prices()
    
    pressed_key = get_key()
    if pressed_key == button_cancel then
        waiting_loops = play_video_wait_time * 10
        return mode_choose 
    end
    if pressed_key == button_begin then return mode_start_filling end

    update_balance()

    return mode_fundraising
end

start_filling_mode = function()
    volume = balance / price_p[1]
    show_start_filling(balance, volume)
    run_stop()
    check_open_lid()
    turn_light(0, animation.idle)
    
    pressed_key = get_key()
    if pressed_key == button_begin then
        start_balance = balance
        start_fluid_flow_sensor(volume * 1000)
        set_current_state(balance)
        return mode_filling
    end

    update_balance()

    return mode_start_filling
end

filling_mode = function()

    check_open_lid()

    pressed_key = get_key()

    if pressed_key == button_pause then
        volume = volume - get_volume() / 1000
        hardware:SendPause()
        return mode_thanks

        --[[
        is_paused = not is_paused
        if is_paused == false then 
            start_fluid_flow_sensor(volume * 1000)
        else
            volume = volume - get_volume() / 1000
            hardware:SendPause()
        end
        ]]
        
    end

    --if is_paused == false then 
    show_filling(balance)
    turn_light(0, animation.one_button)
    balance = price_p[1] * (volume - get_volume() / 1000)
    --end
    
    if balance <= 0.01 then
        balance = 0
        return mode_thanks
    end
    
    if get_sensor_active() == false then
        confirm_transaction(balance)
        balance = 0
        waiting_loops = 0
        return mode_apology
    end

    return mode_filling
end

thanks_mode = function()
    check_open_lid()
    run_stop()
    show_thanks()
    

    if is_waiting_receipt == false then
        confirm_transaction(balance)
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
            send_receipt(post_position, cash_balance, electronical_balance, 0)
            is_waiting_receipt = false
            increment_cars() 
            return mode_fundraising
        end
        waiting_loops = waiting_loops - 1
    
    else
        send_receipt(post_position, cash_balance, electronical_balance, 0)
        is_waiting_receipt = false
        increment_cars()
        waiting_loops = play_video_wait_time * 10
	    return mode_choose
    end

    return mode_thanks
end

apology_mode = function()
    run_stop()
    show_apology()
    check_open_lid()

    if waiting_loops <= 0 then waiting_loops = apology_mode_seconds * 10 end
    waiting_loops = waiting_loops - 1

    if waiting_loops <= 0 then
        waiting_loops = play_video_wait_time * 10 
        return mode_choose 
    end
    return mode_apology
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

    --[[
    if is_paused == true then
        filling:Set("play.visible", "true")
        filling:Set("pause.visible", "false")
    else
        filling:Set("play.visible", "false")
        filling:Set("pause.visible", "true")
    end
    ]]
    

    balance_int = math.ceil(balance_rur)
    filling:Set("balance.value", balance_int)
    set_progressbar(balance_rur / start_balance)
    waiting_loops_progress = waiting_loops_progress + 1
    if waiting_loops_progress >= 12 then waiting_loops_progress = 0 end
    set_progress_indicator(waiting_loops_progress)
    filling:Display()
end

show_thanks =  function()
    thanks:Display()
end

show_apology =  function()
    apology:Display()
end

set_progressbar = function(progress_rur)
    progress_int = math.ceil(progress_rur * 14)
    for i = 1, progress_int do
        filling:Set("progressbar_" .. i .. ".visible",  "true")
    end
    for i = progress_int + 1, 14 do
        filling:Set("progressbar_" .. i .. ".visible",  "false")
    end
end

set_progress_indicator = function(progress_rur)
    filling:Set("progress.index", math.floor(progress_rur))
end

set_time = function(hours_rur, minutes_rur)
    choose:Set("hours.value",  hours_rur)
    choose:Set("minutes.value",  minutes_rur)
    fundraising:Set("hours.value",  hours_rur)
    fundraising:Set("minutes.value",  minutes_rur)
    keyboard:Set("hours.value",  hours_rur)
    keyboard:Set("minutes.value",  minutes_rur)
    wait:Set("hours.value",  hours_rur)
    wait:Set("minutes.value",  minutes_rur)
    start_filling:Set("hours.value",  hours_rur)
    start_filling:Set("minutes.value",  minutes_rur)
    filling:Set("hours.value",  hours_rur)
    filling:Set("minutes.value",  minutes_rur)
    thanks:Set("hours.value",  hours_rur)
    thanks:Set("minutes.value",  minutes_rur)
    apology:Set("hours.value",  hours_rur)
    apology:Set("minutes.value",  minutes_rur)

    if minutes_rur <= 9 then
        choose:Set("minutes_zero.visible", "true")
        fundraising:Set("minutes_zero.visible", "true")
        keyboard:Set("minutes_zero.visible", "true")
        wait:Set("minutes_zero.visible", "true")
        start_filling:Set("minutes_zero.visible", "true")
        filling:Set("minutes_zero.visible", "true")
        thanks:Set("minutes_zero.visible", "true")
        apology:Set("minutes_zero.visible", "true")
    else
        choose:Set("minutes_zero.visible", "false")
        fundraising:Set("minutes_zero.visible", "false")
        keyboard:Set("minutes_zero.visible", "false")
        wait:Set("minutes_zero.visible", "false")
        start_filling:Set("minutes_zero.visible", "false")
        filling:Set("minutes_zero.visible", "false")
        thanks:Set("minutes_zero.visible", "false")
        apology:Set("minutes_zero.visible", "false")
    end
end

set_weather = function(negative_rur, degrees_rur, fraction_rur)
    if negative_rur then
        choose:Set("plus.visible", "false")
        fundraising:Set("plus.visible", "false")
        keyboard:Set("plus.visible", "false")
        wait:Set("plus.visible", "false")
        start_filling:Set("plus.visible", "false")
        filling:Set("plus.visible", "false")
        thanks:Set("plus.visible", "false")
        apology:Set("plus.visible", "false")

        choose:Set("minus.visible", "true")
        fundraising:Set("minus.visible", "true")
        keyboard:Set("minus.visible", "true")
        wait:Set("minus.visible", "true")
        start_filling:Set("minus.visible", "true")
        filling:Set("minus.visible", "true")
        thanks:Set("minus.visible", "true")
        apology:Set("minus.visible", "true")
        --if degrees_rur >= 10 then
        --    main_screen:Set("minus.position", "165;1005")
        --else
        --    main_screen:Set("minus.position", "165;990")
        --end
    else
        --if degrees_rur >= 10 then
        --    main_screen:Set("plus.position", "165;1005")
        --else
        --    main_screen:Set("plus.position", "165;990")
        --end
        choose:Set("minus.visible", "false")
        fundraising:Set("minus.visible", "false")
        keyboard:Set("minus.visible", "false")
        wait:Set("minus.visible", "false")
        start_filling:Set("minus.visible", "false")
        filling:Set("minus.visible", "false")
        thanks:Set("minus.visible", "false")
        apology:Set("minus.visible", "false")

        choose:Set("plus.visible", "true")
        fundraising:Set("plus.visible", "true")
        keyboard:Set("plus.visible", "true")
        wait:Set("plus.visible", "true")
        start_filling:Set("plus.visible", "true")
        filling:Set("plus.visible", "true")
        thanks:Set("plus.visible", "true")
        apology:Set("plus.visible", "true")
    end
    choose:Set("degrees.value",  degrees_rur)
    fundraising:Set("degrees.value",  degrees_rur)
    keyboard:Set("degrees.value",  degrees_rur)
    wait:Set("degrees.value",  degrees_rur)
    start_filling:Set("degrees.value",  degrees_rur)
    filling:Set("degrees.value",  degrees_rur)
    thanks:Set("degrees.value",  degrees_rur)
    apology:Set("degrees.value",  degrees_rur)
    choose:Set("fraction.value",  fraction_rur)
    fundraising:Set("fraction.value",  fraction_rur)
    keyboard:Set("fraction.value",  fraction_rur)
    wait:Set("fraction.value",  fraction_rur)
    start_filling:Set("fraction.value",  fraction_rur)
    filling:Set("fraction.value",  fraction_rur)
    thanks:Set("fraction.value",  fraction_rur)
    apology:Set("fraction.value",  fraction_rur)
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

send_receipt = function(post_pos, cash, electronical, qrMoney)
    hardware:SendReceipt(post_pos, cash, electronical, qrMoney)
    cash_balance = 0
    electronical_balance = 0
end

increment_cars = function()
    hardware:IncrementCars()
end

run_fillin = function()
    set_current_state(balance)
    run_program(1)
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

request_transaction_separated = function(balance)
    return hardware:RequestTransactionSeparated(balance)
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

get_volume = function()
    return hardware:GetVolume() 
end

start_fluid_flow_sensor = function(volume_rur)
    hardware:StartFluidFlowSensor(math.ceil(volume_rur))
end

get_sensor_active = function()
    return hardware:GetSensorActive()
end

get_can_play_video = function()
    return hardware:GetCanPlayVideo()
end

set_can_play_video = function(canPlayVideo)
    hardware:SetCanPlayVideo(canPlayVideo)
end

get_is_playing_video = function()
    return hardware:GetIsPlayingVideo()
end

set_is_playing_video = function(isPlayingVideo)
    hardware:SetIsPlayingVideo(isPlayingVideo)
end

need_to_open_lid = function()
    val = hardware:GetOpenLid()
    if val>0 then return true end
    return false
end

check_open_lid = function()
    if need_to_open_lid() then
        printMessage("LID OPENED ...")
        run_program(11)
        printMessage("LID OPENED :(")
        smart_delay(9500)
        run_program(0)
        printMessage("LID CLOSED :)")
    end
end

get_process_id = function()
    return hardware:GetProcessId()
end