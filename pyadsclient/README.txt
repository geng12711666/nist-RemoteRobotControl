Pre-requisites
- Make sure that the license is activated for the PLC (The TC indication light on PLC should be green rather than blue)
  If license not activated follow "License renew for PLC"

- Make sure the connection route has been add for the TwinCAT
  Follow "Add the PLC route" to add route

**** License renew for PLC ****
1. Open TwintCAT XAE by right clicking the TwinCAT icon on the bottom right, select TwinCAT XAE (TcXaeShell)
2. Open the project 
3. Navigate to the Solution explorer on the left, double click the "License" in teh SYSTEM subtree
4. Click "7 Days Trial License" under License Activation
5. Restart TwinCAT by clicking the "Restart TwinCAT system" icon (the green icon with a gear)


**** Add the PLC route ****
1. Right click the TwinCAT icon on the bottom right, select "router" -> "edit routes"
2. Click the "Add" 
3. Enter the PLC IP for a search (for the 5G connection case, search for the modem IP and do a port forwarding to the PLC in the modem)
4. Click "Add route"
5. Disable the "secure ADS" option, and put Administrator/1 for the User/Password







