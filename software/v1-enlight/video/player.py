import vlc
import sys
import time
import os
from pynput import mouse
from threading import Thread
import threading

global mouse_thread_ready
mouse_thread_ready = threading.Event()

global listener
mouse_pressed = False

def safe_exit(mousebtn, error_message=None, exit_code=0):
    if error_message:
        print(error_message)
    
    if mousebtn and listener:
        listener.stop()
        mouse_thread.join()
    
    exit(exit_code)

def on_click(x, y, button, pressed):
    global mouse_pressed, listener
    if pressed:
        mouse_pressed = True
        listener.stop()

def listen_to_mouse():
    global listener, mouse_thread_ready
    mouse_thread_ready.set()
    with mouse.Listener(on_click=on_click) as listener:
        listener.join()

def play_video(media_player, file_path, repeat=False, mousebtn=False) -> bool:
    media = media_player.get_instance().media_new(file_path)
    media_player.set_media(media)

    # Установка режима "На весь экран"
    media_player.set_fullscreen(True)

    media_player.play()

    while media_player.get_state() == vlc.State.NothingSpecial:
        time.sleep(1)

    while media_player.get_state() != vlc.State.Ended:
        time.sleep(1)
        if mousebtn and mouse_pressed:
            media_player.stop()
            return True

    media_player.set_position(0.0)
    media_player.stop()
    return False


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python player.py <file_path1> <file_path2> ... [--repeat] [--mousebtn]")
        exit()

    file_paths = [arg for arg in sys.argv[1:] if arg not in ['--repeat', '--mousebtn']]

    if not file_paths:
        safe_exit(mousebtn, "Error: No video files provided!", 1)

    repeat = '--repeat' in sys.argv
    mousebtn = '--mousebtn' in sys.argv

    if mousebtn:
        mouse_thread = Thread(target=listen_to_mouse, daemon=True)
        mouse_thread.start()

    mouse_thread_ready.wait()

    player = vlc.Instance("--vout x11")
    media_player = player.media_player_new()

    isEnded = False

    while not isEnded:
        for file_path in file_paths:
            if not os.path.exists(file_path):
                safe_exit(mousebtn, f"Error: File '{file_path}' does not exist!", 1)
            isEnded = play_video(media_player, file_path, repeat, mousebtn)
            if isEnded:
                break

        if not repeat:
            break

    if mousebtn:
        listener.stop()
        mouse_thread.join()
        