# wakeup-curtains
Device to let in sunlight and wake me up. No more hiding behind those blackout curtains!

## Parts:
The schematics can be found in the Docs folder. The electronics enclosure and stepper mounts can be found here:
https://www.thingiverse.com/thing:4943305
*Note: the stepper mounts are specifically measured to my curtains, but the boring electronics box is universal!*

## Libraries utilized:

1. [Unistep2](https://github.com/reven/Unistep2)
      - Non-blocking library for 28BYJ-48 stepper motors
2. [PubSubClient](https://pubsubclient.knolleary.net/)
     - MQTT client I mainly use for logging activity
     - Before deploying my own MQTT broker, I used [HiveMQ](http://www.hivemq.com/demos/websocket-client/) for testing.
3. [FauxmoESP](https://github.com/vintlabs/fauxmoESP)
   - Allows control via Amazon Echo
