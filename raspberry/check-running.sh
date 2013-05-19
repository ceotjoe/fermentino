#!/bin/sh
PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/local/games:/usr/games

if [ "$(ps ax | grep -v grep | grep fermentino.py)" != "" ]; then
    echo "fermentino running, everything is fine"
    exit 0
else
    if [ -e '/var/www/do_not_run_fermentino' ]; then
        echo "do_not_run_fermentino file exists, not restarting"
    else
        # check if serial port exists
        if [ -e '/dev/ttyACM0' ]; then
            echo "Serial port found, but fermentino not running, restarting fermentino"
            echo "fermentino script not found running by CRON, restarting fermentino" >> /home/fermentino/logs/stderr.txt
            # overwrite stdout, append to stderr
            # -u flag causes stdout to write to file immediately and not cache output
            python -u /home/fermentino/fermentino.py 1> /home/fermentino/logs/stdout.txt 2>>/home/fermentino/logs/stderr.txt &
        else
            uptime=$(cat /proc/uptime)
            uptime=${uptime%%.*}

            if [ $uptime -gt 600 ]; then
                echo "Serial port not found by CRON, restarting Raspberry Pi" >> /home/fermentino/logs/stderr.txt
                sudo reboot
            else
                echo "Serial port not found by CRON, but will not reboot in first 10 minutes after boot" >> /home/fermentino/logs/stderr.txt
            fi
        fi
    fi
fi

exit 0

# This script checks whether the python script is running. If it is not running, it starts the script.
# A dontrun file is written if the script is stopped manually, so CRON will not restart it.
# When the Raspberry Pi has lost it's serial port, it will restart the pi. But only after more then 10 minutes after booting.
# To disable this file, remove from the fermentino user's crontab or create the do_not_run_brewpi file: touch /var/www/do_not_run_fermentino