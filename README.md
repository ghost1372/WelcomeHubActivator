# WelcomeHubActivator

A PS5 ELF payload that enables the Welcome Hub.

## Requirements

- A PS5 payload loader capable of running this ELF and a console running supported firmware `10.00`–`13.60`.
- You need to perform fake account activation and fake sign-in beforehand. You can use [**ProsperoManager**](https://github.com/notmaj0r/ProsperoMgr/releases/) to complete both steps.

> [!IMPORTANT]
> This payload supports PS5 firmware 10.00–13.60. Welcome Hub was introduced in firmware 10.00+, so it is not available on firmware versions below 10.00. Therefore, the Welcome Hub cannot be activated on those versions.

> [!WARNING]
> If Welcome Hub fails to activate, your console may need a **System Feature Update**. This is different from a regular **System Software Update**.
>
> 1. Disable automatic downloads and installation of system software updates.
> 2. Connect your console to the internet.
> 3. Check for available **System Feature Updates** and install them if necessary.
> 4. **Important:** Make sure you do not download or install a regular System Software Update, as this may update your console's firmware.

## How to Use

1. Download the latest `WelcomeHubActivator.elf` from the [GitHub Releases](https://github.com/ghost1372/WelcomeHubActivator/releases) page.
2. Send the ELF to your PS5 using [Payload Sender](https://github.com/ghost1372/payloadsender) or [payload manager](https://github.com/itsPLK/ps5-payload-manager) payload.
3. Wait for the **Welcome Hub activated** confirmation message.
4. Sign out of your account and sign in again to see the changes.


## How to compile

> [!Note]
> You can use Windows and WSL2 to compile this project.

1. Install or prepare the [PS5 Payload SDK](https://github.com/ps5-payload-dev/sdk).

2. From the project directory, run:

   ```sh
    make
   ```

The output is `WelcomeHubActivator.elf`.

> [!TIP] 
> Creates a backup of `/system_data/priv/mms/app.db` at `/system_data/priv/mms/app.bckdb` before making any modifications, allowing the original database to be restored if anything goes wrong.

<img width="1600" height="900" alt="image" src="https://github.com/user-attachments/assets/129af97d-9bba-460b-9b35-c505863018a3" />

<img width="739" height="415" alt="image" src="https://github.com/user-attachments/assets/834854b5-0972-485d-86c6-d78c25990207" />
