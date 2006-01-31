package org.globus.usage.report;

import java.sql.DriverManager;
import java.sql.Connection;
import java.sql.Statement;
import java.sql.ResultSet;

import java.util.HashMap;
import java.util.TreeMap;
import java.util.Comparator;
import java.util.Map;
import java.util.Iterator;
import java.util.StringTokenizer;
import java.net.InetAddress;
import java.util.Date;
import java.util.Calendar;
import java.util.GregorianCalendar;
import java.text.SimpleDateFormat;

public class ServiceReport {

    private Map services = new TreeMap(new StringComparator());
    private Map ipLookupTable = new HashMap();

    private void discoverDomains() {
        System.out.println("Computing domains...");

        Iterator iter = services.entrySet().iterator();
        while(iter.hasNext()) {
            Map.Entry entry = (Map.Entry)iter.next();

            ServiceEntry serviceEntry = (ServiceEntry)entry.getValue();

            Iterator ipIter = 
                serviceEntry.getUniqueIPList().keySet().iterator();
            while(ipIter.hasNext()) {
                String ip = (String)ipIter.next();
                
                IPEntry ipEntry = (IPEntry)ipLookupTable.get(ip);
                if (ipEntry == null) {
                    ipEntry = IPEntry.getIPEntry(ip);
                    ipLookupTable.put(ip, ipEntry);
                }

                serviceEntry.addDomain(ipEntry.getDomain());
            }
        }
    }
    
    private void displayTotals() {
        Iterator iter = services.entrySet().iterator();
        while(iter.hasNext()) {
            Map.Entry entry = (Map.Entry)iter.next();

            ServiceEntry serviceEntry = (ServiceEntry)entry.getValue();

            System.out.println(entry.getKey() + ", " + 
                               serviceEntry.getStandaloneCount() + ", " +
                               serviceEntry.getServletCount() + ", " +
                               serviceEntry.getUniqueIPCount() + ", " +
                               serviceEntry.getSortedDomains());
        }
        
        System.out.println();
        System.out.println("Total unqiue services: " + services.size());
    }
    
    private void compute(String listOfServices,
                         int containerType,
                         String ip) {
        // handle the case where all the service names do not fit in the
        // packet
        if (listOfServices.length() >= 1445) {
            int pos = listOfServices.lastIndexOf(',');
            if (pos != -1) {
                listOfServices = listOfServices.substring(0, pos);
            }
        } 

        boolean isPrivateAddress = ServiceEntry.isPrivateAddress(ip);
        if (ip.startsWith("/")) {
            ip = ip.substring(1);
        }
        
        StringTokenizer tokens = new StringTokenizer(listOfServices, ",");
        while(tokens.hasMoreTokens()) {
            String serviceName = tokens.nextToken();
            
            ServiceEntry entry = (ServiceEntry)services.get(serviceName);
            if (entry == null) {
                entry = new ServiceEntry();
                services.put(serviceName, entry);
            }

            switch (containerType) {
            case 1: 
                entry.standalone(); break;
            case 2: 
                entry.servlet(); break;
            default: 
                entry.other(); break;
            }

            if (!isPrivateAddress) {
                entry.addAddress(ip);
            }
        }
    }

    private static class StringComparator implements Comparator {
        public int compare(Object o1, Object o2) {
            String s1 = (String)o1;
            String s2 = (String)o2;
            return s1.compareTo(s2);
        }
    }

    public static void main(String[] args) throws Exception {
    
        String driverClass = "org.postgresql.Driver";
        String url = "jdbc:postgresql://pgsql.mcs.anl.gov:5432/usagestats?user=allcock&password=bigio";
        String baseQuery = "select service_list,container_type,ip_address from java_ws_core_packets where event_type = 1 and ";
        
        Connection con = null;

        String inputDate = args[0];
        int n = Integer.parseInt(args[1]);
        String containerType = null;
        if (args.length > 2) {
            containerType = args[2];
            baseQuery += " container_type = " + containerType + " and ";
        }
        
        SimpleDateFormat dateFormat = new SimpleDateFormat("yyyy-MM-dd");

        try {
            Class.forName(driverClass);

            con = DriverManager.getConnection(url);

            Date date = dateFormat.parse(inputDate);
            Calendar calendar = dateFormat.getCalendar();

            if (n < 0) {
                calendar.add(Calendar.DATE, n);
                n = -n;
            }

            Date startDate = calendar.getTime();
            calendar.add(Calendar.DATE, n);
            Date endDate = calendar.getTime();

            String startDateStr = dateFormat.format(startDate);
            String endDateStr = dateFormat.format(endDate);
            String timeFilter = "send_time >= '" + startDateStr + 
                "' and send_time < '" + endDateStr + "'";
                
            System.out.println("Generating per-service report between " + 
                               startDateStr +
                               " and " + endDateStr);
            if (containerType != null) {
                System.out.println("Container type: " + containerType);
            }

            String query = baseQuery + timeFilter;

            Statement stmt = con.createStatement();

            ResultSet rs = stmt.executeQuery(query);

            ServiceReport r = new ServiceReport();

            while (rs.next()) {
                r.compute(rs.getString(1), rs.getInt(2), rs.getString(3));
            }

            rs.close();
            stmt.close();

            r.discoverDomains();
            r.displayTotals();

        } finally {
            if (con != null) {
                con.close();
            }
        }
        
    }
    
    
}

