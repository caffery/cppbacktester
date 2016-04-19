//
//  BalanceSet.cpp
//  cppbacktester
//
//  Created by Jeongwon on 4/19/16.
//  Copyright © 2016 Jeongwon. All rights reserved.
//

#include "BalanceSet.hpp"


std::auto_ptr<BalanceSet> BalanceSet::_pInstance;
std::string BalanceSet::CASH("Cash");
std::string BalanceSet::TOTAL("Total");


BalanceSet& BalanceSet::instance(void) {
    if ( _pInstance.get() == 0)
        _pInstance.reset(new BalanceSet());
    return *_pInstance;
}


void BalanceSet::initialize(const boost::gregorian::date& dt,
                            const double init,
                            const std::vector<std::string>& names_) {
    
    assert(names.empty());  // initialize has to be run only once
    
    names = names_;

    // initial amount has to be positive
    if(init < 0)
        throw BalanceException("Negative base capital");
    
    // insert entries
    for (std::vector<std::string>::const_iterator it = names_.begin(); it != names_.end(); it++) {
        BalancePtr pBalance( new Balance(*it, dt, 0) );
        insert(pBalance);
    }
    
    BalancePtr pCASH(new Balance(CASH, dt, init));
    insert(pCASH);
    
    BalancePtr pTOTAL(new Balance(TOTAL, dt, init));
    insert(pTOTAL);
}


void BalanceSet::print(void) const {
    // Print total balance
    
    name_pair its = equal_range(TOTAL);
    for (BalanceSet::const_iterator it = its.first; it != its.second; it++) {
        (*it)->print();
    }
    return;
}

/*
struct update_balance {
    update_balance(double new_balance_) : new_balance(new_balance_){}
    
    void operator()(boost::shared_ptr<Balance> pEOD)
    {
        pEOD->balance = new_balance;
    }
    
private:
    double new_balance;
};

double BalanceSet::_update_capital(const std::string& name, const boost::gregorian::date& dt, const double diff){
    // 1. search name
    // 2. if exists, add diff / if not, insert new dt
    // 2. its date has to be either equal to or smaller than pExe->dt()
    
    if(empty())
        throw BalanceException("BalanceSet empty; cannot update capital");
    
    try {
        name_pair its = equal_range(name);
        iterator it = its.second; it--;
        
        const boost::gregorian::date last_date = (*it)->dt;
        const double last_value = (*it)->balance;
        
        if (last_date == dt) {
            // if already exists, modify the balance
            modify(it, update_balance(last_value + diff));
        } else {
            // if not, insert one
            BalancePtr pBalance(new Balance(name, dt, last_value + diff));
            insert(pBalance);
        }
        
        return last_value + diff;
        
    } catch (std::exception& ex ) {
        
        std::cerr << ex.what() << std::endl;
        exit(EXIT_FAILURE);
        
    }
    
}

void BalanceSet::update_capital(const Execution* pExe){
    
    double diff;
    if (pExe->action() == "Bought") {
        diff = -(pExe->price() * pExe->size());
    }
    else if (pExe->action() ==  "Sold") {
        diff = pExe->price() * pExe->size();
    }
    else if (pExe->action() ==  "Shorted") {
        diff = pExe->price() * pExe->size();
    }
    else if (pExe->action() ==   "Covered") {
        diff = -(pExe->price() * pExe->size());
    }
    else {
        throw BalanceException("Unknown type of execution");
    }
    
    _update_capital(CASH, pExe->dt(), diff);
    
    //    for (const_iterator it = begin(); it != end(); it++) {
    //        (*it)->print();
    //    }
    
    return;
}

void BalanceSet::update_capital(const boost::gregorian::date& dt, const PositionSet& openPos){
    // 1. Cash update
    // 2. Open position EOD update
    
    double EOD_total = 0;
    
    EOD_total += _update_capital(CASH, dt, 0);
    
    for (int i = 0; i < names.size(); i++) {
        
        std::string name = names[i];
        PositionSet::by_symbol::const_iterator symbol_key_end = openPos.get<1>().find(name);
        
        double eod_vaue;
        if (symbol_key_end == openPos.get<1>().end()) {
            // if name is not in open position, add 0
            eod_vaue = 0;
            BalancePtr pBalance(new Balance(name, dt, eod_vaue));
            insert(pBalance);
        } else {
            // else, add close price
            eod_vaue = Series::EODDB::instance().get(name).at(dt).close;
            BalancePtr pBalance(new Balance(name, dt, eod_vaue));
            insert(pBalance);
        }
        EOD_total += eod_vaue;
    }
    
    BalancePtr pTotal(new Balance(TOTAL, dt, EOD_total));
    insert(pTotal);
    
    //    std::cout << "EOD summary" << std::endl;
    //    for (const_iterator it = begin(); it != end(); it++) {
    //        (*it)->print();
    //    }
    
    return;
}


std::map<boost::gregorian::date, double> BalanceSet::monthly(const std::set<boost::gregorian::date>& EOMdate) const {
    name_pair its = equal_range(TOTAL);
    std::map<boost::gregorian::date, double> EOD;
    for (BalanceSet::const_iterator it = its.first; it != its.second; it++) {
        EOD.insert( std::pair<boost::gregorian::date, double>((*it)->dt, (*it)->balance) );
    }
    
    std::map<boost::gregorian::date, double> EOM;
    for (std::set<boost::gregorian::date>::const_iterator pdate = EOMdate.begin(); pdate != EOMdate.end(); pdate++) {
        boost::gregorian::date dt = (*pdate);
        EOM.insert( *EOD.find(dt) );
    }
    
    return EOM;
}

std::vector<double> BalanceSet::monthly_ret(const std::set<boost::gregorian::date>& m_date) const {
    
    std::map<boost::gregorian::date, double> m_total = monthly(m_date);
    
    std::map<boost::gregorian::date, double>::const_iterator it = m_total.begin();
    std::vector<double> result;
    double prev = it->second; it++;
    for (; it != m_total.end(); it++) {
        double now = it->second;
        double ret = (now-prev)/prev * 100;
        result.push_back(ret);
        prev = it->second;
    }
    return result;
}

void BalanceSet::export_to_csv(void) const {
    std::ofstream myfile;
    myfile.open ("Balance.csv");
    
    // Header
    myfile << "Date";
    for (int i = 0; i < names.size(); i++) {
        myfile << "," << names[i];
    }
    myfile << "," << CASH << "," << TOTAL << "\n";
    
    
    // Content
    by_date::const_iterator it_date = get<1>().begin();
    by_date::const_iterator it_end = get<1>().end();
    boost::gregorian::date date;
    
    while (it_date != it_end) {
        date = (*it_date)->dt;
        date_pair its = by_date::equal_range(date);
        std::map<std::string, double> temp;
        for (by_date::const_iterator it = its.first; it != its.second; it++) {
            temp.insert( std::pair<std::string, double>((*it)->name, (*it)->balance) );
        }
        
        myfile << date << ",";
        for (int i = 0; i < names.size(); i++) {
            myfile << temp.at(names[i]) << ",";
        }
        myfile << temp.at(CASH) << "," << temp.at(TOTAL) << "\n";
        
        it_date = its.second;
    }
    
    myfile.close();
    return;
}
*/