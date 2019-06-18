#include <QCoreApplication>
#include <QtDebug>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QRegExp>
#include <QSqlDatabase>
#include <QSqlDriver>
#include <QSqlError>
#include <QSqlQuery>

QStringList split_ignoring_quotes(QString);

int main(int argc, char *argv[])
{
  QCoreApplication a(argc, argv);
  
  QTextStream out(stdout);
  out << QDir::currentPath() << endl;
  
  QString state("wa");
  QString year("2019");
  
  const QString driver("QSQLITE");
  QSqlDatabase db = QSqlDatabase::addDatabase(driver);
  db.setDatabaseName(QString("../create_senate_sqlite/sqlite_files/" + year + "_" + state + ".sqlite"));
  
  QString state_full("");
  if (state == "qld") { state_full = "Queensland"; }
  if (state == "nsw") { state_full = "New South Wales"; }
  if (state == "vic") { state_full = "Victoria"; }
  if (state == "tas") { state_full = "Tasmania"; }
  if (state == "sa")  { state_full = "South Australia"; }
  if (state == "wa")  { state_full = "Western Australia"; }
  if (state == "nt")  { state_full = "Northern Territory"; }
  if (state == "act") { state_full = "Australian Capital Territory"; }
  
  if (!db.open())
  {
    out << "Couldn't open db" << endl;
    return 1;
  }
  
  QSqlQuery query;
  
  // ~~~~~~ Creation of the basic information table ~~~~~~
  if (!query.exec("CREATE TABLE basic_info (id INTEGER PRIMARY KEY, state TEXT, state_full TEXT, year INTEGER, formal_votes INTEGER, atl_votes INTEGER, btl_votes INTEGER)"))
  {
    out << "Couldn't create info table" << endl;
    return 1;
  }
  
  if (!query.exec("INSERT INTO basic_info VALUES (0, '" + state.toUpper() + "', '" + state_full + "', " + year + ", 0, 0, 0)"))
  {
    out << "Couldn't insert basic info" << endl;
    return 1;
  }
  
  
  // ~~~~~~ Creation of the tables for seats and (seat_)booths ~~~~~~
  
  if (!query.exec("CREATE TABLE booths (id INTEGER PRIMARY KEY, seat TEXT, booth TEXT, lon REAL, lat REAL, formal_votes INTEGER)"))
  {
    out << "Couldn't create booth table" << endl;
    return 1;
  }
  
  if (!query.exec("CREATE TABLE seats (id INTEGER PRIMARY KEY, seat TEXT, formal_votes INTEGER)"))
  {
    out << "Couldn't create seat table" << endl;
    return 1;
  }
  
  QFile in_booths("../create_senate_sqlite/aec_files/" + year + "_booths.csv");
  
  // Two passes through the booths file: one for seats, one for booths.
  
  // Pass for seats:
  QStringList seats;
  QList<long long> seats_formal_votes;
  QString prev_seat("");
  int seat_ct = 0;
  
  if (in_booths.open(QIODevice::ReadOnly))
  {
    QTextStream in(&in_booths);
    in.readLine();
    in.readLine();
    
    db.transaction();
    query.prepare("INSERT INTO seats VALUES(?, ?, ?)");
    
    while (!in.atEnd())
    {
      QString line = in.readLine();
      QStringList cells = line.split(",");
      
      if (cells.at(0).toLower() == state)
      {
        if (cells.at(2) != prev_seat)
        {
          query.addBindValue(seat_ct);
          query.addBindValue(cells.at(2));
          query.addBindValue(0);
          
          if (!query.exec())
          {
            out << "Couldn't insert seat" << endl;
            return 1;
          }
          
          seats.append(cells.at(2));
          seats_formal_votes.append(0);
          seat_ct++;
          
          prev_seat = cells.at(2);
        }
      }
    }
    
    in_booths.close();
    
    if (!db.commit())
    {
      out << "Couldn't commit seats" << endl;
      return 1;
    }
  }
  
  
  // Pass through booths.csv for polling places:
  // these will be stored in the form seat_booth.
  QStringList seat_booths;
  QList<long long> seat_booths_formal_votes;
  int booth_ct = 0;
  int listed_booths;
  
  if (in_booths.open(QIODevice::ReadOnly))
  {
    QTextStream in(&in_booths);
    in.readLine();
    in.readLine();
    
    db.transaction();
    query.prepare("INSERT INTO booths VALUES(?, ?, ?, ?, ?, ?)");
    
    while (!in.atEnd())
    {
      QString line = in.readLine();
      QStringList cells = line.split(",");
      
      if (cells.at(0).toLower() == state)
      {
        QString seat_booth(cells.at(2) + "_" + cells.at(5));
        query.addBindValue(booth_ct);
        query.addBindValue(cells.at(2));
        query.addBindValue(seat_booth);
        
        int num_cells = cells.length();
        double lon, lat;
        bool valid_coord;
        lon = cells.at(num_cells - 1).toDouble(&valid_coord);
        lat = cells.at(num_cells - 2).toDouble(&valid_coord);
        
        if (!valid_coord)
        {
          lon = 0.;
          lat = 0.;
        }
        
        query.addBindValue(lon);
        query.addBindValue(lat);
        query.addBindValue(0);
        
        if (!query.exec())
        {
          out << "Couldn't insert booth" << endl;
          return 1;
        }
        
        seat_booths.append(seat_booth);
        seat_booths_formal_votes.append(0);
        booth_ct++;
      }
    }
    
    in_booths.close();
    
    if (!db.commit())
    {
      out << "Couldn't commit booths" << endl;
      return 1;
    }
    
    listed_booths = booth_ct;
  }
  else
  {
    out << "Couldn't open booths file" << endl;
    return 1;
  }
  
  
  // ~~~~~~ Creation of the table for groups (parties) ~~~~~~
  
  // Start by reading the official list of party abbreviations.
  
  QFile in_parties("../create_senate_sqlite/aec_files/" + year + "_parties.csv");
  
  QStringList party_names_1;  // PartyNm
  QStringList party_names_2;  // RegisteredPartyAb
  QStringList party_abbrevs;  // PartyAb
  
  if (in_parties.open(QIODevice::ReadOnly))
  {
    QTextStream in(&in_parties);
    in.readLine();
    in.readLine();
    
    while (!in.atEnd())
    {
      QString line = in.readLine();
      QStringList cells = line.split(",");
      
      if (line.indexOf("\"") >= 0)
      {
        cells = split_ignoring_quotes(line);
      }
      
      party_abbrevs.append(cells.at(1));
      party_names_1.append(cells.at(3));
      party_names_2.append(cells.at(2));
    }
    
    if (year == "2019")
    {
      party_abbrevs.append("LPNP");
      party_names_1.append("Liberal & Nationals");
      party_names_2.append("Liberal & Nationals");
      
      party_abbrevs.append("ALP");
      party_names_1.append("Labor/Country Labor");
      party_names_2.append("Labor/Country Labor");
    }
    
    in_parties.close();
  }
  
  // Read through the primaries.csv file to extract parties/groups
  
  if (!query.exec("CREATE TABLE groups (id INTEGER PRIMARY KEY, group_letter TEXT, party TEXT, party_ab TEXT, primaries INTEGER)"))
  {
    out << "Couldn't create groups table" << endl;
    return 1;
  }
  
  QFile in_primaries("../create_senate_sqlite/aec_files/" + year + "_primaries.csv");
  
  int group_ct = 0;
  
  if (in_primaries.open(QIODevice::ReadOnly))
  {
    QTextStream in(&in_primaries);
    in.readLine();
    in.readLine();
    
    db.transaction();
    query.prepare("INSERT INTO groups VALUES(?, ?, ?, ?, ?)");
    
    while (!in.atEnd())
    {
      QString line = in.readLine();
      QStringList cells = split_ignoring_quotes(line);
      
      
      if (cells.at(0).toLower() == state &&
          cells.at(3).toInt() == 0)
      {
        // ATL group.
        query.addBindValue(group_ct);
        query.addBindValue(cells.at(1));
        
        // *** Need to handle quotes here ***
        
        if (cells.at(5) == "")
        {
          query.addBindValue("Group " + cells.at(1));
          query.addBindValue("Gp" + cells.at(1));
        }
        else
        {
          int party_i = party_names_1.indexOf(cells.at(5));
          if (party_i < 0)
          {
            party_i = party_names_2.indexOf(cells.at(5));
          }
          
          if (party_i < 0)
          {
            query.addBindValue("Group " + cells.at(1));
            query.addBindValue("Gp" + cells.at(1));
          }
          else
          {
            query.addBindValue(cells.at(5));
            query.addBindValue(party_abbrevs.at(party_i));
          }
        }
        
        query.addBindValue(0);
        
        if (!query.exec())
        {
          out << "Couldn't insert group " << cells.at(5) << endl;
          return 1;
        }
        
        group_ct++;
      }
    }
    
    if (!db.commit())
    {
      out << "Couldn't commit groups" << endl;
    }
    
    in_primaries.close();
  }
  
  int num_atl = group_ct;
  
  
  // ~~~~~~ Creation of the table for candidates ~~~~~~
  
  if (!query.exec("CREATE TABLE candidates (id INTEGER PRIMARY KEY, group_letter TEXT, group_pos INTEGER, party TEXT, party_ab TEXT, candidate TEXT, primaries INTEGER)"))
  {
    out << "Couldn't create cands table" << endl;
    return 1;
  }
  
  int cand_ct = 0;
  
  if (in_primaries.open(QIODevice::ReadOnly))
  {
    QTextStream in(&in_primaries);
    in.readLine();
    in.readLine();
    
    db.transaction();
    query.prepare("INSERT INTO candidates VALUES(?, ?, ?, ?, ?, ?, ?)");
    
    while (!in.atEnd())
    {
      QString line = in.readLine();
      //QStringList cells = line.split(",");
      QStringList cells = split_ignoring_quotes(line);
      
      if (cells.at(0).toLower() == state &&
          cells.at(3).toInt() != 0 &&
          cells.at(3).toInt() < 100)
      {
        // Candidate
        query.addBindValue(cand_ct);
        query.addBindValue(cells.at(1));
        query.addBindValue(cells.at(3).toInt());
        query.addBindValue(cells.at(5));
        
        if (cells.at(5) == "")
        {
          query.addBindValue(QString("Gp%1").arg(cells.at(1)));
        }
        else
        {
          int party_i = party_names_1.indexOf(cells.at(5));
          if (party_i < 0)
          {
            party_i = party_names_2.indexOf(cells.at(5));
          }
          
          if (party_i < 0)
          {
            query.addBindValue(QString("Gp%1").arg(cells.at(1)));
          }
          else
          {
            query.addBindValue(party_abbrevs.at(party_i));
          }
        }
        
        query.addBindValue(cells.at(4));
        query.addBindValue(0);
        
        if (!query.exec())
        {
          out << "Couldn't insert candidate" << endl;
          return 1;
        }
        
        cand_ct++;
      }
    }
    
    if (!db.commit())
    {
      out << "Couldn't commit candidates" << endl;
    }
    
    in_primaries.close();
  }
  
  int num_btl = cand_ct;
  
  // ~~~~~~ Creation of the preference tables ~~~~~~
  
  QFile in_file("../create_senate_sqlite/aec_files/" + year + "_prefs_" + state + ".csv");
  
  for (int j = 0; j < 2; j++)
  {
    bool doing_atl = (j == 0);
    
    if (doing_atl)
    {
      out << "Starting pass for ATL" << endl;
    }
    else
    {
      out << "Starting pass for BTL" << endl;
    }
    
    QString table_name = doing_atl ? "atl" : "btl";
    int max_prefs = doing_atl ? num_atl : num_btl;
    int pref_offset = doing_atl ? 0 : num_atl;
    
    QString create_text("CREATE TABLE " + table_name + " (id INTEGER PRIMARY KEY, seat_id INTEGER, booth_id INTEGER, num_prefs INTEGER");
    
    for (int i = 0; i < max_prefs; i++)
    {
      create_text += QString(", P") + QString().setNum(i + 1);
    }
    
    for (int i = 0; i < max_prefs; i++)
    {
      create_text += QString(", Pfor") + QString().setNum(i);
    }
    create_text += ")";
    
    out << create_text << endl;
    
    if (!query.exec(create_text))
    {
      out << "Couldn't create " << table_name << " table";
      return 1;
    }
    
    if (in_file.open(QIODevice::ReadOnly))
    {
      QTextStream in(&in_file);
      in.readLine();
      
      if (year == "2016") { in.readLine(); }
      
      long long line_ct = 0;
      long btl_ct = 0;
      long atl_ct = 0;
      
      QString sql_prepare("INSERT INTO " + table_name + " VALUES(?, ?, ?, ?");
      for (int i = 0; i < max_prefs; i++)
      {
        sql_prepare += ", ?, ?";
      }
      sql_prepare += ")";
      
      out << sql_prepare << endl;
      
      while (!in.atEnd())
      {
        QString line = in.readLine();
        line.replace("/", "1");
        line.replace("*", "1");
        
        QStringList cells = line.split(",");
        
        if (year != "2016")
        {
          // In 2019, the AEC started putting the state as the first field
          // in the prefs file.
          cells.removeFirst();
        }
        
        QStringList prefs;
        
        if (year == "2016")
        {
          int start_prefs = line.indexOf("\"") + 1;
          line.remove(0, start_prefs);
          line.remove("\"");
          
          QStringList prefs = line.split(",");
        }
        else //if (year == "2019")
        {
          for (int i = 5; i < cells.length(); i++)
          {
            prefs.append(cells.at(i));
          }
          
          // Fill out the rest of the unmarked preferences so that
          // I can keep using my 2016 parser.
          for (int i = prefs.length(); i < num_atl + num_btl; i++)
          {
            prefs.append("");
          }
        }
        
        
        int num_prefs_total = prefs.length();
        
        // Check if it's a valid BTL vote.
        
        QList<int> btl_prefs;
        
        for (int i = num_atl; i < num_prefs_total; i++)
        {
          if (prefs.at(i) == "")
          {
            btl_prefs.append(0);
          }
          else
          {
            btl_prefs.append(prefs.at(i).toInt());
          }
        }
        
        bool valid_btl = true;
        
        for (int i = 1; i <= 6; i++)
        {
          if (btl_prefs.indexOf(i) < 0)
          {
            valid_btl = false;
            break;
          }
          
          if (btl_prefs.indexOf(i) != btl_prefs.lastIndexOf(i))
          {
            valid_btl = false;
            break;
          }
        }
        
        if (valid_btl)
        {
          btl_ct++;
        }
        else
        {
          atl_ct++;
        }
        
        if (( valid_btl && !doing_atl) ||
            (!valid_btl &&  doing_atl))
        {
          QString seat = cells.at(0);
          
          QString booth(cells.at(1));
          if (booth.contains(QRegExp("^PRE_POLL")))    { booth = "PRE_POLL"; }
          if (booth.contains(QRegExp("^PROVISIONAL"))) { booth = "PROVISIONAL"; }
          if (booth.contains(QRegExp("^POSTAL")))      { booth = "POSTAL"; }
          if (booth.contains(QRegExp("^ABSENT")))      { booth = "ABSENT"; }
          
          QString seat_booth = seat + "_" + booth;
          
          int seat_id = seats.indexOf(seat);
          int booth_id = seat_booths.indexOf(seat_booth);
          
          if (seat_id < 0)
          {
            out << "ERROR: Couldn't find seat id for " << seat << endl;
            return 1;
          }
          
          seats_formal_votes[seat_id] += 1;
          
          if (booth_id < 0)
          {
            seat_booths.append(seat_booth);
            seat_booths_formal_votes.append(0);
            booth_id = seat_booths.length() - 1;
          }
          
          seat_booths_formal_votes[booth_id] += 1;
          
          if (line_ct % 100000 == 0)
          {
            out << QString().setNum(line_ct) << endl;
            
            if (line_ct > 0)
            {
              if (!db.commit())
              {
                out << "couldn't commit" << endl;
                return 1;
              }
            }
            
            db.transaction();
            if (!query.prepare(sql_prepare))
            {
              out << "couldn't prepare??" << endl;
              return 1;
            }
          }
          
          // abtl -- Above or below the line
          QList<int> abtl_prefs;
          QList<int> abtl_prefs_ordered;
          
          // The AEC's preference files include all the numbers written in the formal
          // votes, including cases of duplicates, sequences missing a number, etc.
          // We want to include in the database only the valid preferences.
          int num_valid_prefs = 0;
          
          QList<int> this_prefs;
          
          for (int i = 0; i < max_prefs; i++)
          {
            if (prefs.at(i + pref_offset) == "")
            {
              this_prefs.append(999);
            }
            else
            {
              this_prefs.append(prefs.at(i + pref_offset).toInt());
            }
          }
          
          for (int i = 1; i <= max_prefs; i++)
          {
            if (this_prefs.indexOf(i) < 0)
            {
              num_valid_prefs = i - 1;
              break;
            }
            
            if (this_prefs.indexOf(i) != this_prefs.lastIndexOf(i))
            {
              num_valid_prefs = i - 1;
              break;
            }
            
            num_valid_prefs = i;
          }
          
          
          for (int i = 0; i < max_prefs; i++)
          {
            if (this_prefs.at(i) <= num_valid_prefs)
            {
              abtl_prefs.append(this_prefs.at(i));
            }
            else
            {
              abtl_prefs.append(999);
            }
          }
          
          for (int i = 1; i <= num_valid_prefs; i++)
          {
            int party_i = abtl_prefs.indexOf(i);
            abtl_prefs_ordered.append(party_i);
          }
          
          for (int i = abtl_prefs_ordered.length(); i < max_prefs; i++)
          {
            abtl_prefs_ordered.append(999);
          }
          
          query.addBindValue(line_ct);
          query.addBindValue(seat_id);
          query.addBindValue(booth_id);
          query.addBindValue(num_valid_prefs);
          
          for (int i = 0; i < max_prefs; i++)
          {
            query.addBindValue(abtl_prefs_ordered.at(i));
          }
          
          for (int i = 0; i < max_prefs; i++)
          {
            query.addBindValue(abtl_prefs.at(i));
          }
          
          if (!query.exec())
          {
            out << "Error at insert exec" << endl;
            out << db.lastError().text() << endl;
            out << "breaking" << endl;
            qDebug() << db.lastError();
            return 1;
          }
          
          line_ct++;
        }
      }
      
      
      if (!db.commit())
      {
        out << "couldn't commit" << endl;
      }
      
      in_file.close();
      
      // Fill in the remaining booths that were not included
      // in the booths.csv file.
      db.transaction();
      query.prepare("INSERT INTO booths VALUES(?, ?, ?, ?, ?, ?)");
      int num_booths = seat_booths.length();
      
      for (int i = listed_booths; i < num_booths; i++)
      {
        query.addBindValue(i);
        QString seat(seat_booths.at(i));
        seat.remove(QRegExp("_.*"));
        query.addBindValue(seat);
        query.addBindValue(seat_booths.at(i));
        query.addBindValue(0.);
        query.addBindValue(0.);
        query.addBindValue(0);
        
        if (!query.exec())
        {
          out << "couldn't bind new booth" << endl;
          return 1;
        }
      }
      
      listed_booths = num_booths;
      
      if (!db.commit())
      {
        out << "Couldn't commit new booths" << endl;
        return 1;
      }
      
      out << QString().setNum(line_ct) << endl;
      out << "ATL: " + QString().setNum(atl_ct) + ", BTL: " + QString().setNum(btl_ct) << endl;
      
      out << "Trying to enter primary vote totals into the groups table" << endl;
      
      if (query.exec("SELECT P1, COUNT(P1) FROM " + table_name + " GROUP BY P1"))
      {
        QList<int> primaries_groups;
        QList<long long> primaries;
        
        while (query.next())
        {
          primaries_groups.append(query.value(0).toInt());
          primaries.append(query.value(1).toLongLong());
        }
        
        if (!db.transaction())
        {
          out << "Couldn't start transaction??" << endl;
        }
        
        QString primaries_table = doing_atl ? "groups" : "candidates";
        
        if (!query.prepare("UPDATE " + primaries_table + " SET primaries = ? WHERE id = ?"))
        {
          out << "Couldn't prepare setting primary" << endl;
          qDebug() << db.lastError();
          return 1;
        }
        
        for (int i = 0; i < primaries.length(); i++)
        {
          query.addBindValue(primaries.at(i));
          query.addBindValue(primaries_groups.at(i));
          
          out << primaries_groups.at(i) << ": " << primaries.at(i) << endl;
          
          if (!query.exec())
          {
            out << "Couldn't update primaries" << endl;
            return 1;
          }
        }
        
        if (!db.commit())
        {
          out << "Couldn't commit primaries" << endl;
          return 1;
        }
      }
      else
      {
        out << "Query to get primary votes failed." << endl;
        return 1;
      }
      
      if (!doing_atl)
      {
        // ~~~~~ Add formal vote total to the basic_info table ~~~~~
        if (!query.exec(QString("UPDATE basic_info SET formal_votes = %1, atl_votes = %2, btl_votes = %3 WHERE id = 0")
                        .arg(atl_ct + btl_ct).arg(atl_ct).arg(btl_ct)))
        {
          out << "Couldn't update formal votes in basic_info table" << endl;
          return 1;
        }
        
        // ~~~~~ Add formal vote totals to the divisions table ~~~~~
        db.transaction();
        
        if (!query.prepare("UPDATE seats SET formal_votes = ? WHERE id = ?"))
        {
          out << "Couldn't prepare setting formal votes for divisions" << endl;
          return 1;
        }
        
        for (int i = 0; i < seats.length(); i++)
        {
          query.addBindValue(seats_formal_votes.at(i));
          query.addBindValue(i);
          
          if (!query.exec())
          {
            out << "Couldn't update divisions formal votes" << endl;
            return 1;
          }
          
        }
        
        if (!db.commit())
        {
          out << "Couldn't commit divisions formal votes" << endl;
          return 1;
        }
        
        
        // ~~~~~ Add formal vote totals to the booths table ~~~~~
        db.transaction();
        
        if (!query.prepare("UPDATE booths SET formal_votes = ? WHERE id = ?"))
        {
          out << "Couldn't prepare setting formal votes for booths" << endl;
          return 1;
        }
        
        for (int i = 0; i < seat_booths.length(); i++)
        {
          query.addBindValue(seat_booths_formal_votes.at(i));
          query.addBindValue(i);
          
          if (!query.exec())
          {
            out << "Couldn't update booths formal votes" << endl;
            return 1;
          }
          
        }
        
        if (!db.commit())
        {
          out << "Couldn't commit booths formal votes" << endl;
          return 1;
        }
      }
    }
    else
    {
      out << "Oh no\n";
    }
  }
  
  
  
  db.close();
  
  out << "end" << endl;
  return a.exec();
}

QStringList split_ignoring_quotes(QString text)
{
  // At least in my version of Qt, regexes with ?s don't 
  // work properly, so I wrote this slow splitter to handle
  // quotation marks.
  
  int num_chars = text.length();
  bool in_quotes = false;
  QStringList cells;
  int start = 0;
  
  for (int i = 0; i < num_chars; i++)
  {
    QString this_char = text.mid(i, 1);
    if (this_char == "\"")
    {
      in_quotes = !in_quotes;
    }
    else
    {
      if (!in_quotes && this_char == ",")
      {
        cells.append(text.mid(start, i - start).replace('"', ""));
        start = i + 1;
      }
    }
  }
  
  cells.append(text.mid(start, num_chars - start).replace('"', ""));
  return cells;
}
