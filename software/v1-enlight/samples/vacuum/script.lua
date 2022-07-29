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

    min_electron_balance = 10
    max_electron_balance = 900
    max_cash_balance = 900

    keyboard_pressed_left = false       -- Утверждает то, что пользователь нажал на кнопку клавиатуры при вводе суммы
    keyboard_pressed_right = false

    fundraiser_active_left = false      -- Утверждает, что в настоящий момент происходит оплата для левого / правого экрана. Необходимо для того, чтобы деньги приходили куда надо
    fundraiser_active_right = false

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
    price_p[2] = 0
    price_p[3] = 0
    price_p[4] = 0

    init_prices()
    printMessage("Prices: " .. price_p[0] .. " " .. price_p[1] .. " " .. price_p[2] .. " " .. price_p[3] .. " " .. price_p[4])
    
    mode_welcome = 0        -- Велком
    mode_choose = 10        -- Выбор способа оплаты
    mode_fundraising = 20   -- Наличная оплата
    mode_keyboard = 30      -- Клавиатура для ввода суммы
    mode_wait = 40          -- Ожидание оплаты
    mode_work = 50          -- Работа
    mode_thanks = 60        -- Благодарности
    mode_apology = 70       -- Извинения

    real_ms_per_loop = 100
    pressed_key = 0
    
    current_right_mode = mode_welcome
    current_left_mode = mode_welcome

    version = "1.0.0"

    printMessage("dia generic wash firmware v." .. version)
    smart_delay(1)
    return 0
end

loop = function()
    pressed_key = get_key()
    current_left_mode = left_display_mode(current_left_mode)
    current_right_mode = right_display_mode(current_right_mode)
    real_ms_per_loop = smart_delay(100)
    return 0
end

init_prices = function()
    price_p[1] = get_price(1)
    price_p[2] = get_price(2)
    price_p[3] = get_price(3)
    price_p[4] = get_price(4)
end

-- Mode

right_display_mode = function(new_mode)
    printMessage("left_display_mode: " .. new_mode)
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
    printMessage("right_display_mode: " .. new_mode)
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
    run_stop()
    turn_light(0, animation.idle)
    smart_delay(1000 * welcome_mode_seconds)
    return mode_choose
end

left_welcome_mode = function()
    left_show_welcome()
    run_stop()
    turn_light(0, animation.idle)
    smart_delay(1000 * welcome_mode_seconds)
    return mode_choose
end

right_choose_mode = function()
    right_show_choose(balance_right)
    run_stop()
    turn_light(0, animation.idle)

    if pressed_key == cash_button_right and fundraiser_active_left == false then
        fundraiser_active_right = true
        return mode_fundraising
    end
    if pressed_key == cashless_button_right then return mode_keyboard end

    return mode_choose
end

left_choose_mode = function()
    left_show_choose(balance_left)
    run_stop()
    turn_light(0, animation.idle)

    if pressed_key == cash_button_left and fundraiser_active_right == false then
        fundraiser_active_left = true
        return mode_fundraising
    end
    if pressed_key == cashless_button_left then return mode_keyboard end

    return mode_choose
end

right_fundraising_mode = function()
    right_show_fundraising(balance_right)
    run_stop()
    turn_light(0, animation.idle)

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
    run_stop()
    turn_light(0, animation.idle)

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

    run_stop()
    turn_light(0, animation.idle)

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
    
    run_stop()
    turn_light(0, animation.idle)

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
    run_stop()
    turn_light(0, animation.idle)

    if is_transaction_started == false then   -- Запуск ожидания транзакции
        waiting_loops_right = wait_mode_seconds * 10;

        request_transaction(electron_balance_right)
        electron_balance_right = 0
        is_transaction_started = true
    end

    status = get_transaction_status()
    update_balance()

    if balance_right > 0.99 then
        if status ~= 0 then
            abort_transaction()
        end
        is_transaction_started = false
        fundraiser_active_right = false
        return mode_work
    end

    if waiting_loops_right <= 0 then -- Если счетчик истек, то возращаем на экран выбора. Надо бы добавить условие имения кардридера
        is_transaction_started = false
	    if status ~= 0 then -- Если запущена активная транзакция, то абортируем ее
	        abort_transaction()
	    end
        fundraiser_active_right = false
        return mode_choose
    end

    if status == 0 and is_transaction_started == true then    -- Если транзакция не идет, то возращаем на выбор
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
    run_stop()
    turn_light(0, animation.idle)

    if is_transaction_started == false then   -- Запуск ожидания транзакции
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

    if waiting_loops_left <= 0 then -- Если счетчик истек, то возращаем на экран выбора. Надо бы добавить условие имения кардридера
        is_transaction_started = false
	    if status ~= 0 then -- Если запущена активная транзакция, то абортируем ее
	        abort_transaction()
	    end
        fundraiser_active_left = false
        return mode_choose
    end

    if status == 0 and is_transaction_started == true then    -- Если транзакция не идет, то возращаем на выбор. обавить условие наскардридер
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
    right_show_work(balance_right)

    set_current_state(balance_right + balance_left)

    charge_balance_right(price_p[1])
    run_stop()
    turn_light(0, animation.idle)

    if balance_right <= 0.01 then return mode_thanks end

    return mode_work
end

left_work_mode = function()
    left_show_work(balance_left)

    set_current_state(balance_right + balance_left)

    charge_balance_left(price_p[1])
    run_stop()
    turn_light(0, animation.idle)

    if balance_left <= 0.01 then return mode_thanks end

    return mode_work
end

right_thanks_mode = function()
    right_show_thanks()
    
    balance_right = 0
    set_current_state(balance_left + balance_right)

    send_receipt(post_position, cash_balance_right, electronical_balance_right)
    cash_balance_right = 0
    electronical_balance_right = 0

    return mode_choose
end

left_thanks_mode = function()
    left_show_thanks()
    turn_light(1, animation.one_button)

    balance_left = 0
    set_current_state(balance_left + balance_right)

    send_receipt(post_position, cash_balance_left, electronical_balance_left)
    cash_balance_left = 0
    electronical_balance_left = 0

    return mode_choose
end
--[[
right_apology_mode = function()
    
end

left_apology_mode = function()
    
end
]]

-- Show

left_show_welcome = function()
    left_disable_visibility()
    main_screen:Display()
end

left_show_choose = function(balance_rur)
    left_disable_visibility()
    left_charge_balance(balance_rur)

    main_screen:Set("programs_left.position", "700;544")
    main_screen:Set("payment_method_left.position", "1300;590")
    if fundraiser_active_right == false then main_screen:Set("button_cash_left.position", "1360;587") end
    if  hascardreader() then main_screen:Set("button_cashless_left.position", "1500;587") end
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

    main_screen:Set("button_1_left.position", "780;627")
    main_screen:Set("button_2_left.position", "780;760")
    main_screen:Set("button_3_left.position", "780;893")
    main_screen:Set("button_4_left.position", "913;627")
    main_screen:Set("button_5_left.position", "913;760")
    main_screen:Set("button_6_left.position", "913;893")
    main_screen:Set("button_7_left.position", "1046;627")
    main_screen:Set("button_8_left.position", "1046;760")
    main_screen:Set("button_9_left.position", "1046;893")
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

left_show_work = function(balance_rur)
    left_disable_visibility()
    left_charge_balance(balance_rur)

    main_screen:Set("console_left.position", "700;635")
    main_screen:Set("button_pause_left.position", "1500;587")

    main_screen:Display()
end

left_show_thanks = function()
    left_disable_visibility()
    left_charge_balance(0)
    main_screen:Set("thanks_left.position", "700;580")
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

    main_screen:Set("programs_right.position", "700;8")
    main_screen:Set("payment_method_right.position", "1300;101")
    if fundraiser_active_left == false then main_screen:Set("button_cash_right.position", "1360;77") end
    if hascardreader() then main_screen:Set("button_cashless_right.position", "1500;77") end
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

right_show_work = function(balance_rur)
    right_disable_visibility()
    right_charge_balance(balance_rur)

    main_screen:Set("console_right.position", "700;102")
    main_screen:Set("button_pause_right.position", "1500;77")

    main_screen:Display()
end

right_show_thanks = function()
    right_disable_visibility()
    right_charge_balance(0)
    main_screen:Set("thanks_right.position", "700;79")
    main_screen:Display()
end

right_show_apology = function()
    right_disable_visibility()
    right_charge_balance(0)
    main_screen:Set("apology_right.position", "700;92")
    main_screen:Display()
end

right_disable_visibility = function()
    main_screen:Set("balance_right.position", "2000;2000")
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
end

left_disable_visibility = function()
    main_screen:Set("balance_left.position", "2000;2000")
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
end

right_charge_balance = function(balance_rur)
    balance_int = math.ceil(balance_rur)
    main_screen:Set("balance_right.value", balance_int)
end

left_charge_balance = function(balance_rur)
    balance_int = math.ceil(balance_rur)
    main_screen:Set("balance_left.value", balance_int)
end












-- Util

get_mode_by_pressed_key = function(current_mode)
    pressed_key = get_key()
    
    if pressed_key < 1 then return -1 end
    if is_working_mode(current_mode) and current_mode~=mode_work then return mode_pause end
    return mode_work + 1
end

get_key = function()    -- Получает нажатую клиентом кнопку
    return hardware:GetKey()
end

smart_delay = function(ms)  -- Задержка на столько то милисекунд
    return hardware:SmartDelay(ms)
end

get_price = function(key)   -- Стоймость указанной программы
    return registry:GetPrice(key)
end

turn_light = function(rel_num, animation_code)  -- Включает свет на указанном реле
    hardware:TurnLight(rel_num, animation_code)
end

send_receipt = function(post_pos, cash, electronical)   -- Выдает чек
    hardware:SendReceipt(post_pos, cash, electronical)
end

increment_cars = function()
    hardware:IncrementCars()
end

run_pause = function()
    run_program(2)
end

run_stop = function()
    run_program(0)
end

run_program = function(program_num)     -- Запускает указанную программу
    hardware:TurnProgram(program_num)
end

request_transaction = function(money)   -- Запрос на транзакцию
    return hardware:RequestTransaction(money)
end

get_transaction_status = function()     -- 0 - транзакции нет, > 0 -  ожидание средств в размере возращаемого значения
    return hardware:GetTransactionStatus()
end

abort_transaction = function()          -- Прерывает транзакцию
    return hardware:AbortTransaction()
end

set_current_state = function(current_balance)  -- Устанавливает текущее состояние для сервера. Баланс будет отображаться в приложении
    return hardware:SetCurrentState(math.floor(current_balance))
end

update_balance = function()     -- Обновление баланса
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

charge_balance_left = function(price)   -- Уменьшает баланс в соответсвии с указанной ценой в минуту (?)
    balance_left = balance_left - price * real_ms_per_loop / 60000
    if balance_left < 0 then balance_left = 0 end
end

charge_balance_right = function(price)  -- Уменьшает баланс в соответсвии с указанной ценой в минуту (?)
    balance_right = balance_right - price * real_ms_per_loop / 60000
    if balance_right < 0 then balance_right = 0 end
end

is_working_mode = function(mode_to_check)
  if mode_to_check >= mode_work and mode_to_check<mode_work+10 then return true end
  return false
end

hascardreader = function()
  return hardware:HasCardReader()
end
