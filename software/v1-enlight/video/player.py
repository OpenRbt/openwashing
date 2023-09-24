import vlc
import sys
import time
from pynput import mouse
from threading import Thread

global listener
mouse_pressed = False

def on_click(x, y, button, pressed):
    global mouse_pressed, listener
    if pressed:
        mouse_pressed = True
        listener.stop()

def listen_to_mouse():
    global listener
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
        print("Usage: python script.py <file_path1> <file_path2> ... [--repeat] [--mousebtn]")
        exit()

    file_paths = [arg for arg in sys.argv[1:] if arg not in ['--repeat', '--mousebtn']]
    repeat = '--repeat' in sys.argv
    mousebtn = '--mousebtn' in sys.argv

    if mousebtn:
        mouse_thread = Thread(target=listen_to_mouse)
        mouse_thread.start()

    player = vlc.Instance("--no-xlib")
    media_player = player.media_player_new()

    isEnded = False

    while not isEnded:
        for file_path in file_paths:
            isEnded = play_video(media_player, file_path, repeat, mousebtn)
            if isEnded:
                break

        if not repeat:
            break

    if mousebtn:
        listener.stop()
        mouse_thread.join()
        