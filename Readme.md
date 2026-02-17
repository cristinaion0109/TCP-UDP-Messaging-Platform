ION CRISTINA GABRIELA
321CC
# TEMA 2 PCOM

## Descriere

In cadrul acestei teme am implementat o aplicatie in care serverul primeste 
mesaje de la clientii udp, pe care mai apoi le distribuie catre clientii tcp
in functie de topicurile la care acestia sunt abonati.

## Structura

Fisierul subscriber.c:

- gestioneaza comenzile primite de la tastatura pentru fiecare client, respectiv 
mesajele primite de la server

- clientul se conecteaza la un server printr-un ip si un port

- mai intai, trimite catre server id-ul sau

- sunt gestionate comenzile primite de la tastatura. Pentru exit, se trimite 
un mesaj catre server, utilizand functia send_msg cu type-ul 2 asociat. Pentru 
subscribe si unsubscribe, se procedeaza asemanator, utilizand type-ul 0 pentru 
subscribe si type-ul 1 pentru unsubscribe

- in cazul unui mesaj primit de la server, se verifica daca s-a primit STOP(cazul
in care serverul a primit exit de la tasatura, adica clientul trebuie inchis), iar 
in caz contrar se afiseaza mesajul ce contine informatiile despre topicurile la care
clientul este abonat

Fisierul server.c:

- gestioneaza comenzile primite de la tastatura, de la clientii udp si de la clientii 
tcp

- se pastreaza o lista a clientilor, in care se stocheaza cu ajutorul unei structuri 
socketul, id-ul, starea(1 - activ, 0 - inactiv), o lista cu topicurile primite atunci
cand se da subscribe, o lista cu topicurile in forma completa si 2 contoare pentru 
topicuri

- la primirea comenzii exit de la tastura, se inchide atat serverul, cat si clientii
(fiecare client primeste mesaj de STOP)

- la conectarea unui nou client tcp, acesta se adauga in lista de clienti, verificandu-se 
daca clientul respectiv este deja conectat

- la primirea unui mesaj udp, se proceseaza mesajul in functie de tip, se face update la 
lista de topicuri complete, in functie de topicurile primite atunci cand se da subscribe, 
iar apoi se trimite mesajul procesat catre toti clientii abonati la topicul respectiv sau 
la un topic ce contine wildcard compatibil

- pentru a verifica potrivirea intre topicul primit de la udp si topicurile la care este 
abonat clientul, se utilizeaza functia search_match si search_match_aux(compara recursiv lista 
de cuvinte ce contine wildcarduri si lista de cuvinte ce contine topicul)

- atunci cand se primeste mesaj de la clientii tcp activi, care au dat comenzi de subscribe/
unsubscribe/exit, se extrage tipul comenzii si topicul, identificand clientul in lista pe baza 
socketului. Daca comanda este de tip subscribe(type = 0), atunci se adauga topicul in subscribed_topics, 
incrementand contorul de topicuri. Daca comanda este de tip unsubscribe(type = 1), se verifica daca 
exista o potrivire intre noul topic primit si elementele din lista subscribed_topics_complete, eliminand 
topicurile relevante atat din subscribed_topics, cat si din subscribed_topics_complete. Daca se primeste 
exit(type = 2), se afiseaza un mesaj corespunzator, inchizand socketul clientului respectiv.

Bibliografie:
- https://github.com/VladStefanDieaconu/PC-Protocoale-de-Comunicatii/blob/master/TEME/tema2/udp_client.py

Pentru a-mi forma o idee despre cum ar trebui sa arate rezolvarea temei, m-am uiat la implementarea de mai
sus.
De asemenea, am pornit de la scheletul prezentat in laboratorul 7.




