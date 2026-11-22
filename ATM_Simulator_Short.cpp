/*
 * ATM Simulator - Short web version (C++11, no extra library)
 * Compile (Windows): g++ -std=c++11 ATM_Simulator_Short.cpp -o atm -lws2_32
 * Compile (Linux/Mac): g++ -std=c++11 ATM_Simulator_Short.cpp -o atm
 * Run: .\atm.exe  then open http://localhost:8080
 * DEMO accounts: 1001/1234 (Savings), 1002/5678 (Savings), 1003/4321 (Current)
 */
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#ifdef _WIN32
  #include <winsock2.h>
  typedef SOCKET sock_t;
  #define CLOSE_SOCKET closesocket
#else
  #include <csignal>
  #include <netinet/in.h>
  #include <sys/socket.h>
  #include <sys/time.h>
  #include <unistd.h>
  typedef int sock_t;
  #define CLOSE_SOCKET close
#endif
using namespace std;

string money(double v) {                       // e.g. "Rs. 25000.00"
    ostringstream out;
    out << "Rs. " << fixed << setprecision(2) << v;
    return out.str();
}

bool toNumber(const string& text, double& result) {   // true only for a valid number
    char* end;
    result = strtod(text.c_str(), &end);
    return !text.empty() && *end == '\0' && fabs(result) < 1e9;
}

// ---------- Transaction: one entry in the history ----------
class Transaction {
private:
    string type, timestamp;
    double amount;                 // positive = money in, negative = money out
    static int totalTransactions;  // shared by ALL Transaction objects
public:
    Transaction(const string& type, double amount) : type(type), amount(amount) {
        time_t now = time(nullptr);
        char buffer[32];
        strftime(buffer, sizeof(buffer), "%d-%b %I:%M %p", localtime(&now));
        timestamp = buffer;
        ++totalTransactions;
    }
    string display() const {
        ostringstream out;
        out << left << setw(16) << timestamp << setw(13) << type << right << setw(14)
            << ((amount >= 0 ? "+" : "-") + money(fabs(amount)));
        return out.str();
    }
};
int Transaction::totalTransactions = 0;

// ---------- Account: abstract base class ----------
class Account {
private:                           // encapsulation: data is hidden
    int accountNumber;
    string holderName, pin;
    double balance;
    vector<Transaction> history;
    int failedAttempts;
    bool blocked;
    static int totalAccounts;

    void record(const string& type, double signedAmount) {
        history.push_back(Transaction(type, signedAmount));
    }

public:
    Account(int no, const string& name, const string& pin, double opening)
        : accountNumber(no), holderName(name), pin(pin), balance(opening),
          failedAttempts(0), blocked(false) { ++totalAccounts; }
    virtual ~Account() { --totalAccounts; }

    int getAccountNumber() const { return accountNumber; }
    string getHolderName() const { return holderName; }
    double getBalance() const { return balance; }
    bool isBlocked() const { return blocked; }
    int getRemainingAttempts() const { return 3 - failedAttempts; }

    bool verifyPin(const string& entered) {        // 3 wrong attempts block the account
        if (blocked) return false;
        if (entered == pin) { failedAttempts = 0; return true; }
        if (++failedAttempts >= 3) blocked = true;
        return false;
    }

    // Each operation returns true if done, otherwise 'message' says why not.
    bool deposit(double amount, string& message) {
        if (amount <= 0) { message = "Amount must be greater than zero."; return false; }
        balance += amount;
        record("Deposit", amount);
        return true;
    }
    bool withdraw(double amount, string& message) {
        if (amount <= 0) { message = "Amount must be greater than zero."; return false; }
        // virtual functions: the derived class decides the value (runtime polymorphism)
        if (amount > getWithdrawalLimit()) {
            message = "Amount exceeds the limit of " + money(getWithdrawalLimit()) + ".";
            return false;
        }
        if (amount > getAvailableFunds()) {
            message = "Insufficient balance. Available: " + money(getAvailableFunds());
            return false;
        }
        balance -= amount;
        record("Withdrawal", -amount);
        return true;
    }
    bool transferOut(double amount, string& message) {
        if (amount <= 0) { message = "Amount must be greater than zero."; return false; }
        if (amount > getAvailableFunds()) {
            message = "Insufficient balance. Available: " + money(getAvailableFunds());
            return false;
        }
        balance -= amount;
        record("Transfer Out", -amount);
        return true;
    }
    void transferIn(double amount) {
        balance += amount;
        record("Transfer In", amount);
    }

    // function overloading: same name, different parameters
    string showMiniStatement() const { return showMiniStatement(5); }
    string showMiniStatement(int count) const {
        ostringstream out;
        out << "MINI STATEMENT\n" << string(43, '-') << "\n";
        if (history.empty()) out << "No transactions yet.\n";
        size_t start = (history.size() > (size_t)count) ? history.size() - count : 0;
        for (size_t i = start; i < history.size(); ++i) out << history[i].display() << "\n";
        out << string(43, '-') << "\nCurrent Balance: " << money(balance);
        return out.str();
    }

    virtual double getWithdrawalLimit() const = 0;   // max per withdrawal
    virtual double getAvailableFunds() const = 0;    // how much can be taken out now
};
int Account::totalAccounts = 0;

// Savings: lower limit, and Rs. 500 must always stay in the account.
class SavingsAccount : public Account {
public:
    SavingsAccount(int no, const string& name, const string& pin, double opening)
        : Account(no, name, pin, opening) {}
    double getWithdrawalLimit() const { return 20000.0; }
    double getAvailableFunds() const { return max(0.0, getBalance() - 500.0); }
};

// Current: higher limit, and an overdraft of Rs. 10,000.
class CurrentAccount : public Account {
public:
    CurrentAccount(int no, const string& name, const string& pin, double opening)
        : Account(no, name, pin, opening) {}
    double getWithdrawalLimit() const { return 50000.0; }
    double getAvailableFunds() const { return getBalance() + 10000.0; }
};

// ---------- Bank: owns the accounts ----------
class Bank {
private:
    vector<Account*> accounts;         // base-class pointers -> polymorphism
public:
    Bank() {                           // DEMO DATA ONLY
        accounts.push_back(new SavingsAccount(1001, "Rahul Sharma", "1234", 25000));
        accounts.push_back(new SavingsAccount(1002, "Priya Verma", "5678", 40000));
        accounts.push_back(new CurrentAccount(1003, "Arjun Mehta", "4321", 60000));
    }
    ~Bank() {
        for (size_t i = 0; i < accounts.size(); ++i) delete accounts[i];   // virtual destructor
    }
    Account* findAccount(int no) const {           // nullptr if not found
        for (size_t i = 0; i < accounts.size(); ++i)
            if (accounts[i]->getAccountNumber() == no) return accounts[i];
        return nullptr;
    }
    bool transfer(Account& sender, int receiverNo, double amount, string& message) {
        Account* receiver = findAccount(receiverNo);
        if (receiver == nullptr) { message = "Receiver account not found."; return false; }
        if (receiver == &sender) { message = "You cannot transfer to your own account."; return false; }
        if (!sender.transferOut(amount, message)) return false;
        receiver->transferIn(amount);
        return true;
    }
};

// ---------- ATM: takes a request from the browser, returns the reply text ----------
class ATM {
private:
    Bank bank;
    Account* currentAccount;           // logged-in account (nullptr if none)
public:
    ATM() : currentAccount(nullptr) {}

    string handle(const string& action, map<string, string>& form) {
        double number = 0, amount = 0;
        string message;

        if (action == "/login") {
            if (!toNumber(form["account"], number)) return "Enter a valid account number.";
            Account* account = bank.findAccount((int)number);
            if (account == nullptr) return "Account not found.";
            if (account->isBlocked()) return "Account is blocked (3 wrong PINs).";
            if (account->verifyPin(form["pin"])) {
                currentAccount = account;
                return "Login successful. Welcome, " + account->getHolderName() + "!";
            }
            if (account->isBlocked()) return "Account is blocked (3 wrong PINs).";
            ostringstream out;
            out << "Invalid PIN. Attempts remaining: " << account->getRemainingAttempts();
            return out.str();
        }

        if (currentAccount == nullptr) return "Please login first.";
        if (action == "/balance")   return "Current Balance: " + money(currentAccount->getBalance());
        if (action == "/statement") return currentAccount->showMiniStatement();
        if (action == "/logout")    { currentAccount = nullptr; return "Logged out."; }

        if (action == "/withdraw" || action == "/deposit") {
            if (!toNumber(form["amount"], amount)) return "Enter a valid amount.";
            bool ok = (action == "/withdraw") ? currentAccount->withdraw(amount, message)
                                              : currentAccount->deposit(amount, message);
            if (!ok) return "Failed: " + message;
            return "Successful! Updated Balance: " + money(currentAccount->getBalance());
        }
        if (action == "/transfer") {
            if (!toNumber(form["receiver"], number) || !toNumber(form["amount"], amount))
                return "Enter a valid receiver account and amount.";
            if (!bank.transfer(*currentAccount, (int)number, amount, message))
                return "Transfer Failed: " + message;
            return "Transfer Successful! Remaining Balance: " + money(currentAccount->getBalance());
        }
        return "Unknown action.";
    }
};

// ---------- The web page shown in the browser ----------
const char* PAGE = R"HTML(<!DOCTYPE html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>ABC Bank ATM</title>
<style>
body{font-family:Georgia,serif;background:#101114;color:#F1EFE8;max-width:420px;margin:20px auto;padding:0 12px}
.card{background:#1C1E24;border-radius:10px;padding:14px;margin:12px 0}
input{width:100%;box-sizing:border-box;padding:9px;margin:4px 0 8px;border-radius:6px;border:1px solid #444;background:#101114;color:#F1EFE8}
button{padding:9px 14px;margin:4px 4px 0 0;border:0;border-radius:6px;background:#185FA5;color:#fff;cursor:pointer}
pre{background:#000;color:#5DCAA5;padding:12px;border-radius:8px;white-space:pre-wrap}
.hide{display:none}
</style></head><body>
<h1>ABC Bank ATM</h1>

<div id="loginBox" class="card">
  <input id="acc" placeholder="Account number (1001)">
  <input id="pin" type="password" placeholder="PIN (1234)">
  <button onclick="login()">Login</button>
</div>

<div id="menuBox" class="hide">
  <div class="card">
    <button onclick="send('/balance')">Balance</button>
    <button onclick="send('/statement')">Mini statement</button>
    <button onclick="logout()">Logout</button>
  </div>
  <div class="card">
    <input id="amt" placeholder="Amount (Rs.)">
    <button onclick="send('/withdraw',{amount:val('amt')})">Withdraw</button>
    <button onclick="send('/deposit',{amount:val('amt')})">Deposit</button>
  </div>
  <div class="card">
    <input id="rec" placeholder="Receiver account number">
    <input id="tamt" placeholder="Amount (Rs.)">
    <button onclick="send('/transfer',{receiver:val('rec'),amount:val('tamt')})">Transfer</button>
  </div>
</div>

<pre id="out">Please login.</pre>

<script>
function val(id){return document.getElementById(id).value}
function post(path,data){
  return fetch(path,{method:'POST',body:new URLSearchParams(data||{})}).then(function(r){return r.text()});
}
function send(path,data){post(path,data).then(function(t){document.getElementById('out').textContent=t})}
function toggle(loggedIn){
  document.getElementById('loginBox').className=loggedIn?'hide':'card';
  document.getElementById('menuBox').className=loggedIn?'':'hide';
}
function login(){
  post('/login',{account:val('acc'),pin:val('pin')}).then(function(t){
    document.getElementById('out').textContent=t;
    if(t.indexOf('Login successful')===0) toggle(true);
  });
}
function logout(){send('/logout'); toggle(false)}
</script></body></html>)HTML";

// ---------- Tiny web server (plain sockets, localhost only) ----------
map<string, string> parseForm(const string& body) {      // "a=1&b=2" -> {a:1, b:2}
    map<string, string> form;
    istringstream in(body);
    string pair;
    while (getline(in, pair, '&')) {
        size_t eq = pair.find('=');
        if (eq != string::npos) form[pair.substr(0, eq)] = pair.substr(eq + 1);
    }
    return form;
}

void handleClient(sock_t client, ATM& atm) {
    string request;
    char buffer[4096];
    size_t headerEnd, length = 0;
    while ((headerEnd = request.find("\r\n\r\n")) == string::npos) {
        int n = recv(client, buffer, sizeof(buffer), 0);
        if (n <= 0) return;                               // idle connection: ignore
        request.append(buffer, n);
    }
    size_t pos = request.find("ontent-Length:");           // matches Content-Length / content-length
    if (pos != string::npos && pos < headerEnd) length = (size_t)atoi(request.c_str() + pos + 14);
    while (request.size() < headerEnd + 4 + length) {
        int n = recv(client, buffer, sizeof(buffer), 0);
        if (n <= 0) break;
        request.append(buffer, n);
    }

    string reply, type = "text/plain";
    if (request.compare(0, 4, "POST") == 0) {
        size_t start = request.find(' ') + 1;
        string path = request.substr(start, request.find(' ', start) - start);
        map<string, string> form = parseForm(request.substr(headerEnd + 4));
        reply = atm.handle(path, form);
    } else {
        reply = PAGE;
        type = "text/html";
    }

    ostringstream response;
    response << "HTTP/1.1 200 OK\r\nContent-Type: " << type << "; charset=utf-8\r\nContent-Length: "
             << reply.size() << "\r\nConnection: close\r\n\r\n" << reply;
    string data = response.str();
    send(client, data.c_str(), (int)data.size(), 0);
}

int main() {
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#else
    signal(SIGPIPE, SIG_IGN);
#endif
    ATM atm;
    sock_t server = socket(AF_INET, SOCK_STREAM, 0);
    int yes = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes));

    sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(8080);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);     // this computer only

    if (bind(server, (sockaddr*)&address, sizeof(address)) != 0 || listen(server, 5) != 0) {
        cout << "Could not start. Is port 8080 already in use?\n";
        return 1;
    }
    cout << "ATM Simulator running. Open http://localhost:8080  (Ctrl+C to stop)\n";

    while (true) {
        sock_t client = accept(server, nullptr, nullptr);
#ifdef _WIN32
        DWORD timeout = 1000;                             // ignore idle browser connections
#else
        timeval timeout = {1, 0};
#endif
        setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
        handleClient(client, atm);
        CLOSE_SOCKET(client);
    }
}
