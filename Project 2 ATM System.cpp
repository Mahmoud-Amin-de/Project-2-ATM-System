// ===========================================================
// ATM System
// ===========================================================
// A console-based ATM simulation for existing bank clients:
// login with an account number + PIN, then Quick Withdraw,
// Normal (custom-amount) Withdraw, Deposit, and Check Balance.
//
// Companion project to the Client & Transactions Management
// System: reuses its data file and record format so both
// programs operate on the same clients.
//   Clients.txt: AccountNumber#//#PinCode#//#Name#//#Phone#//#AccountBalance
//
// Unlike the management system (used by staff, gated by user
// accounts/permissions), this program is used directly by a
// client: login only checks account number + PIN, and every
// logged-in client has access to all four ATM actions.
//
// CurrentClient holds the logged-in client for the rest of the
// session. It's the one piece of shared/global state in this
// program, which is reasonable here since exactly one client is
// ever logged in at a time; DepositBalanceToClientByAccountNumber
// is the single place that updates it, and only once a
// transaction is actually confirmed and saved - so it can never
// drift out of sync with what's on disk.
// ===========================================================

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <iomanip>
#include <limits>

using namespace std;
const string ClientsFileName = "Clients.txt";

struct sClient
{
    string AccountNumber;
    string PinCode;
    string Name;
    string Phone;
    double AccountBalance;

    // Soft-delete flag: kept for compatibility with Clients.txt
    // records written by the Client & Transactions Management
    // System, even though this program never sets it itself.
    bool MarkForDelete = false;
};

enum enATMMainMenuOptions
{
    mnuQuickWithdraw = 1,
    mnuNormalWithdraw = 2,
    mnuDeposit = 3,
    mnuCheckBalance = 4,
    mnuLogout = 5
};

sClient CurrentClient;

void ShowLoginScreen();
void ShowATMMainMenu();
void ShowQuickWithdrawScreen();
void ShowNormalWithdrawScreen();
void ShowDepositScreen();
void ShowCheckBalanceScreen();

// Splits a string into tokens using the given delimiter.
// Empty tokens (e.g. from consecutive delimiters) are skipped.
vector<string> SplitString(string S1, string Delim)
{

    vector<string> vString;

    short pos = 0;
    string sWord; // define a string variable

    // use find() function to get the position of the delimiters
    while ((pos = S1.find(Delim)) != std::string::npos)
    {
        sWord = S1.substr(0, pos); // store the word
        if (sWord != "")
        {
            vString.push_back(sWord);
        }

        S1.erase(0, pos + Delim.length());  /* erase() until position and move to next word. */
    }

    if (S1 != "")
    {
        vString.push_back(S1); // it adds last word of the string.
    }

    return vString;

}

// Parses a single line from the data file into a client record.
sClient ConvertClientLineToRecord(string Line, string Separator = "#//#")
{

    sClient Client;
    vector<string> vClientData;

    vClientData = SplitString(Line, Separator);

    Client.AccountNumber = vClientData[0];
    Client.PinCode = vClientData[1];
    Client.Name = vClientData[2];
    Client.Phone = vClientData[3];
    Client.AccountBalance = stod(vClientData[4]);//cast string to double


    return Client;

}

// Serializes a client record into a single delimited line for storage.
string ConvertClientRecordToLine(sClient Client, string Separator = "#//#")
{

    string stClientRecord = "";

    stClientRecord += Client.AccountNumber + Separator;
    stClientRecord += Client.PinCode + Separator;
    stClientRecord += Client.Name + Separator;
    stClientRecord += Client.Phone + Separator;
    stClientRecord += to_string(Client.AccountBalance);

    return stClientRecord;

}

// Reads a double from input, re-prompting if the entered value
// is not a valid number (clears cin's fail state and discards
// the bad input so the program doesn't get stuck in a loop).
double ReadValidatedDouble(string Prompt)
{
    double Value = 0;
    cout << Prompt;
    cin >> Value;

    while (cin.fail())
    {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << "Invalid input, please enter a numeric value? ";
        cin >> Value;
    }

    return Value;
}

// Same idea as ReadValidatedDouble, for whole-number menu choices.
short ReadValidatedShort(string Prompt)
{
    short Value = 0;
    cout << Prompt;
    cin >> Value;

    while (cin.fail())
    {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << "Invalid input, please enter a number? ";
        cin >> Value;
    }

    return Value;
}

// Reads all client records from disk into memory.
// Returns an empty vector if the file doesn't exist or can't be opened.
vector <sClient> LoadClientsDataFromFile(string FileName)
{

    vector <sClient> vClients;

    fstream MyFile;
    MyFile.open(FileName, ios::in);//read Mode

    if (MyFile.is_open())
    {

        string Line;
        sClient Client;

        while (getline(MyFile, Line))
        {

            Client = ConvertClientLineToRecord(Line);

            vClients.push_back(Client);
        }

        MyFile.close();

    }

    return vClients;

}

// Overwrites Clients.txt with the current in-memory vector.
vector <sClient> SaveClientsDataToFile(string FileName, vector <sClient> vClients)
{

    fstream MyFile;
    MyFile.open(FileName, ios::out);//overwrite

    string DataLine;

    if (MyFile.is_open())
    {

        for (sClient C : vClients)
        {

            if (C.MarkForDelete == false)
            {
                //we only write records that are not marked for delete.
                DataLine = ConvertClientRecordToLine(C);
                MyFile << DataLine << endl;

            }

        }

        MyFile.close();

    }

    return vClients;

}

// Checks a login attempt (account number + PIN) against the client
// file. On success, fills in the matched client's full record.
bool FindClientByAccountNumberAndPinCode(string AccountNumber, string PinCode, sClient& Client)
{

    vector <sClient> vClients = LoadClientsDataFromFile(ClientsFileName);

    for (sClient C : vClients)
    {

        if (C.AccountNumber == AccountNumber && C.PinCode == PinCode)
        {
            Client = C;
            return true;
        }

    }
    return false;

}

// Applies Amount to Client's balance (a negative Amount is a
// withdrawal) once the user confirms, and persists the change.
// Shared by Deposit and both Withdraw screens.
//
// Client is passed by reference and its AccountBalance is only
// ever updated here, inside the confirmed branch - so the
// session's cached balance can't drift out of sync with what's
// actually saved to Clients.txt (it's left untouched if the user
// answers "n", or if the account can't be found).
bool DepositBalanceToClientByAccountNumber(sClient& Client, double Amount, vector <sClient>& vClients)
{

    char Answer = 'n';

    cout << "\n\nAre you sure you want to perform this transaction? y/n ? ";
    cin >> Answer;

    if (Answer == 'y' || Answer == 'Y')
    {

        for (sClient& C : vClients)
        {
            if (C.AccountNumber == Client.AccountNumber)
            {
                C.AccountBalance += Amount;
                Client.AccountBalance += Amount;
                SaveClientsDataToFile(ClientsFileName, vClients);
                cout << "\n\nDone Successfully. New balance is: " << C.AccountBalance;

                return true;
            }

        }

        return false;
    }

    return false;

}

// Keeps re-prompting until the entered amount is a multiple of 5
// (this ATM only dispenses bills in multiples of 5).
double ReadAmountMultipleOfFive()
{
    double Amount = ReadValidatedDouble("\nEnter an amount, multiple of 5's ? ");

    while ((long long)Amount % 5 != 0)
    {
        Amount = ReadValidatedDouble("\nAmount must be a multiple of 5. Enter an amount, multiple of 5's ? ");
    }

    return Amount;
}

double ReadDepositAmount()
{
    double Amount = ReadValidatedDouble("\nEnter a positive Deposit Amount? ");

    while (Amount <= 0)
    {
        Amount = ReadValidatedDouble("Amount must be positive. Enter a positive Deposit Amount? ");
    }

    return Amount;
}

// Validates the full advertised range ([1]-[8] preset amounts,
// [9] Exit) in one place, on top of ReadValidatedShort's numeric check.
short ReadQuickWithdrawOption()
{
    short Choice = ReadValidatedShort("\nChoose what do you want to do from [1] to [9] ? ");

    while (Choice < 1 || Choice > 9)
    {
        cout << "Invalid choice, please choose a number from 1 to 9.\n";
        Choice = ReadValidatedShort("Choose what to withdraw from [1] to [9] ? ");
    }

    return Choice;
}

void ShowQuickWithdrawScreen()
{
    const double QuickAmounts[8] = { 20, 50, 100, 200, 400, 600, 800, 1000 };

    cout << "===========================================\n";
    cout << "\t\tQuick Withdraw\n";
    cout << "===========================================\n";
    cout << "\t[1] 20\t\t[2] 50\n";
    cout << "\t[3] 100\t\t[4] 200\n";
    cout << "\t[5] 400\t\t[6] 600\n";
    cout << "\t[7] 800\t\t[8] 1000\n";
    cout << "\t[9] Exit\n";
    cout << "===========================================\n";
    cout << "Your Balance is " << CurrentClient.AccountBalance << endl;

    short Choice = ReadQuickWithdrawOption();

    if (Choice == 9)
        return;

    double Amount = QuickAmounts[Choice - 1];

    if (Amount > CurrentClient.AccountBalance)
    {
        cout << "\nThe amount exceeds your balance, make another choice.\n";
        cout << "Press any key to continue...";
        system("pause>0");
        system("cls");
        ShowQuickWithdrawScreen();
        return;
    }

    vector <sClient> vClients = LoadClientsDataFromFile(ClientsFileName);
    DepositBalanceToClientByAccountNumber(CurrentClient, Amount * -1, vClients);
}

void ShowNormalWithdrawScreen()
{
    cout << "===========================================\n";
    cout << "\t\tNormal Withdraw Screen\n";
    cout << "===========================================\n";

    double Amount = ReadAmountMultipleOfFive();

    if (Amount > CurrentClient.AccountBalance)
    {
        cout << "\nThe amount exceeds your balance, make another choice.\n";
        cout << "Press any key to continue...";
        system("pause>0");
        system("cls");
        ShowNormalWithdrawScreen();
        return;
    }

    vector <sClient> vClients = LoadClientsDataFromFile(ClientsFileName);
    DepositBalanceToClientByAccountNumber(CurrentClient, Amount * -1, vClients);
}

void ShowDepositScreen()
{
    cout << "===========================================\n";
    cout << "\t\tDeposit Screen\n";
    cout << "===========================================\n";

    double Amount = ReadDepositAmount();

    vector <sClient> vClients = LoadClientsDataFromFile(ClientsFileName);
    DepositBalanceToClientByAccountNumber(CurrentClient, Amount, vClients);
}

void ShowCheckBalanceScreen()
{
    cout << "===========================================\n";
    cout << "\t\tCheck Balance Screen\n";
    cout << "===========================================\n";
    cout << "Your Balance is " << CurrentClient.AccountBalance << endl;
}

void GoBackToMainMenu()
{
    cout << "\n\nPress any key to go back to Main Menu...";
    system("pause>0");
    ShowATMMainMenu();
}

short ReadATMMainMenuOption()
{
    return ReadValidatedShort("Choose what do you want to do? [1 to 5]? ");
}

void PerformATMMainMenuOption(enATMMainMenuOptions MenuOption)
{
    switch (MenuOption)
    {
    case mnuQuickWithdraw:
        system("cls");
        ShowQuickWithdrawScreen();
        GoBackToMainMenu();
        break;

    case mnuNormalWithdraw:
        system("cls");
        ShowNormalWithdrawScreen();
        GoBackToMainMenu();
        break;

    case mnuDeposit:
        system("cls");
        ShowDepositScreen();
        GoBackToMainMenu();
        break;

    case mnuCheckBalance:
        system("cls");
        ShowCheckBalanceScreen();
        GoBackToMainMenu();
        break;

    case mnuLogout:
        // Logging out returns to the Login Screen rather than closing
        // the program, so the next client can sign in.
        system("cls");
        ShowLoginScreen();
        break;

    default:
        cout << "\nInvalid choice, please choose a number from 1 to 5.\n";
        system("pause>0");
        ShowATMMainMenu();
    }
}

void ShowATMMainMenu()
{
    system("cls");
    cout << "===========================================\n";
    cout << "\t\tATM Main Menu Screen\n";
    cout << "===========================================\n";
    cout << "\t[1] Quick Withdraw.\n";
    cout << "\t[2] Normal Withdraw.\n";
    cout << "\t[3] Deposit\n";
    cout << "\t[4] Check Balance.\n";
    cout << "\t[5] Logout.\n";
    cout << "===========================================\n";

    PerformATMMainMenuOption((enATMMainMenuOptions)ReadATMMainMenuOption());
}

void ShowLoginScreen()
{
    bool LoginFailed = false;
    string AccountNumber, PinCode;

    do
    {
        system("cls");
        cout << "\n-----------------------------------\n";
        cout << "\tLogin Screen";
        cout << "\n-----------------------------------\n";

        if (LoginFailed)
            cout << "Invalid Account Number/PinCode!\n";

        cout << "Enter Account Number? ";
        getline(cin >> ws, AccountNumber);

        cout << "Enter Pin? ";
        getline(cin >> ws, PinCode);

        LoginFailed = !FindClientByAccountNumberAndPinCode(AccountNumber, PinCode, CurrentClient);

    } while (LoginFailed);

    ShowATMMainMenu();
}

int main()
{
    ShowLoginScreen();
    system("pause>0");
    return 0;
}