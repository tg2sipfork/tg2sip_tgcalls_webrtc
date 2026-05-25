# TG2SIP WebRTC Fork

Your SIP PBX should be compatible with `OPUS@48000` voice codec.

## Usage

1. Obtain `api_id` and `api_hash` tokens from [my.telegram.org](https://my.telegram.org) and put them in the `settings.ini` file.
2. Login into Telegram with the `gen_db` app.
3. Set SIP server settings in `settings.ini`.
4. Run `prod`.

## Call Routing

SIP -> Telegram calls can be done using 3 extension types:
* `aa_[\s\d]+` — for calls by username
* `\+[\d]+` — for calls by phone number
* `[\d]+` — for calls by Telegram ID (Only known IDs allowed by Telegram API).

All Telegram -> SIP calls will be redirected to the `callback_uri` SIP-URI that can be set in the `settings.ini` file.  
Extra information about the caller's Telegram account will be added into `X-TG-*` SIP tags.



# build \ up \ shell \ restart container with GW
Run manage.sh from root with this arguments:
    
    build    - Build the docker image"
    up       - Start the container (docker compose up -d)"
    shell    - Enter the container shell"
    restart  - Recreate container and enter shell"

#Use 
1. "docker builder prune -a -f"
    for clear docker build cache if need