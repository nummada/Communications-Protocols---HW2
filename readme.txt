README TEMA 2 PC
Nuta Mihaela-Madalina 324CB
Tab width readme: 4

--------------------------------------------------------------------------------
|			   					Surse folosite:                                |
--------------------------------------------------------------------------------
	Pentru fisierele server.c si subscriber.c m-am folosit de laboratorul de tcp
cu multiplexare, cat si de laboratorul de udp pentru a realiza conexiunea cu
clientii udp.
	In server.c se afla implementarea pentru server, in subscriber.c se afla
implementarea pentru subscriber, in helpers.h utilitarul DIE, iar in structs.h
reprezentarea structurilor folosite in tema.

--------------------------------------------------------------------------------
|								Exemple rulare		   						   |
--------------------------------------------------------------------------------
./server 8047
./subscriber ana13  127.0.0.1 8047 pentru un client cu id-ul "ana13".
	Server-ul se poate rula cu ajutorul makefile-ului, insa, pentru clienti,
am folosit terminalul, deoarece am avut nevoie de clienti cu id-uri diferite
pentru testarea implementarii temei.
	Tin sa precizez ca implementarea merge cum trebuie, cel putin, pentru
comenzile de tipul:
python3 udp_client.py --source-port 1234 --input_file three_topics_payloads.json
 --mode random  --delay 100 127.0.0.1 8047


	In cadrul acestei teme, am urmarit urmatorii pasi, pentru server, respectiv
subscriber:
--------------------------------------------------------------------------------
|								Pentru server:		   						   |
--------------------------------------------------------------------------------

Pregatiri conexiuni:
	Am creat un socket pentru tcp si un socket pentru udp. Am completat campurile
pentru ambele structuri de tipul sockaddr_in asociate fiecarul tip de socket cu
sin_family, sin_port si sin_addr. Am facut bind pe ambele socket-uri, iar listen
am facut doar pe socketul tcp (nu stiu sincer de ce, am vazut ca nu merge pe
udp, iar tot ce gaseam legat de functia listen era legat de tcp). Am apelat
functia FD_SET pentru socketul tcp, socketul udp si 0(socket default pentru
stdin). Am setat aceste socket-uri pe variabila read_fds, copie a carei
variabile va fi folosita pentru functia 'select'.

Primire de mesaje pe conexiuni:
	Intr-un while(1), mergand de la socket = 0 la fdmax, am verificat daca
tmp_fds, copie a read_fds este setata pe pozitia actuala (poz = i).

Cazuri variabila i:
	Daca i == sockTcp: inseamna ca am primit o cerere de conexiune pe socket-ul
tcp. Functia accept a intors un nou socket pe care l-am setat, socket specific
fiecarui subscriber.
	Am apelat functia initClient, care imi initializeaza un client si il adauga
in lista. In aceasta functie setez toate campurile structurii de tip client. De
id-ul clientului voi face rost dupa ce clientul va trimite un mesaj imediat dupa
ce acesta a realizat conexiunea (explic in detaliu putin mai tarziu).

	Daca i == 0: inseamna ca am primit un mesaj de la stdin. Serverul poate
primi de la stdin doar comanda "exit", deci, aceasta este singura verificare.
Atunci cand serverul primeste exit, acesta trebuie sa deconecteze mai intai toti
clientii conectati la el. Astfel, apelez functia exitAll care trimite un mesaj
de tip "exit" tuturor clientilor.

	Daca i == sockUdp: inseamna ca am primit un mesaj de la clientii de tip udp,
adica mesaje legate de news din topicuri. In acest caz, doar am apelat functia
recvUdpMessage, care face urmatoarele lucruri:
	-cu ajutorul unei variabile de tip "udpMessage" apeleaza functia recvfrom,
pentru a salva datele primite in aceasta variabila. Acest tip de structura
prezinta un camp care reprezinta topicul care are 50 de caractere, un camp char
care reprezinta dataType si un camp content care contine 1500 de caractere.
	-creez un alt tip de variabila (message, care contine un vector de char-uri)
in care voi ingloba mesajul legat de topic. I-am pus un string la inceput
"infotopic" pentru a sti clientul ce tip de mesaj este, dupa care am concatenat
informatiile asa cum s-a specificat in cerinta. Aici, am folosit functia
initMessajeFromUdp, in care:
	-pentru int, citesc din content prima data semnul (content[0]), dupa care
un uint32_t. Daca semnul este 1, inmultesc numarul cu -1. Pentru short_real,
citesc direct un uint16_t , il impart la 100 si atat. Pentru float, fac exact
la fel ca la int, doar ca impart, dupa, numarul la 10 la puterea content[1+4],
1 pentru semn, 4 pentru cei 4 bytes din numar, pentru a ajunge la pozitia
puterii negative a lui 10. Pentru string, pastrez content-ul exact cum este.
	Dupa ce am folosit aceasta functie, am trimis acest mesaj la toti clientii
activi care sunt abonati la acest topic, cu ajutorul functiei "sendTopicUpdate".
Daca clientii nu sunt activi si sunt abonati la acel topic, cu ajutorul functiei
"saveMessagesForClients" salvez mesajul in lista de mesaje "messages" din
fiecare client.

	Daca i nu este niciun socket recunoscut (tcp, udp, stdin) inseamna: ca am
primit mesaj pe unul dintre socketii reprezentativi pentru fiecare client.
	Astfel, preiau index-ul unui client in functie de socket-ul "i" si verific:
	-daca am primit un mesaj de tipul "id", daca am gasit index-ul (adica este
diferit de -1), atunci verific daca clientul este activ. Daca este activ,
inseamna ca a incercat sa se conecteze un client cu un id al unui alt client
care este activ, deci ii voi refuza conexiunea si il voi sterge din lista. Il
sterg din lista, deoarece la conectare i-am acceptat conexiunea oricum, pentru
ca nu stiam ce id are. Daca este inactiv, creez un mesaj pe care il voi afisa
cu ajutorul functiei printMessageForNewClient. Daca este activ, trimit acelui
client un mesaj prin care ii spun ca acel id este deja utilizat.
	-daca primeste un mesaj de tip "subscribe", verific daca am atins
dimensiunea maxima si fac realloc, dupa care, daca topic-ul deja exist, trimit
unui mesaj clientului prin care ii spun ca topic-ul deja exista, dar a fost
actualizata valoare din campul SF. Daca nu exista, adaug topic-ul in in lista de
topic-uri a clientului.
	-daca primesc o comanda de tipul unsubscribe, sterg acel topic din lista de
topic-uri a clientului cu ajutorul functiei deleteTopic (mut elementele din
vector cu o pozitie la stanga de la pozitia elementului pe care vreau sa-l
sterg).

--------------------------------------------------------------------------------
|								Pentru subscriber:		   					   |
--------------------------------------------------------------------------------
	La nivel de conexiune, am creat un socket pentru conexiuni de tip tcp. Am
apelat functia FD_SET pe socket-ul de tip tcp si pe 0(default pentru stdin).
Am completat sin_family, sin_port in structura de tipul sockaddr_in, dupa care
am apelat functia "connect" pe socket-ul de tip tcp, pentru a ma conecta la
server. Dupa conectarea la server, am trimis un mesaj prin care anunt serverul
ce id am.
	Pentru a primi mesajele, intr-un while(1) am verificat:
	-daca am primit mesaj la stdin cu ajutorul functiei fgets pe stdin.
	Daca am primit o comanda de la stdin de tip "exit", dau break iar executia
se termina.
	Daca am primit o comanda goala, am afisat un mesaj de eroare.
	Daca am primit un mesaj de tip subscribe, am verificat cateva detalii despre
acest mesaj pentru a sti daca este valid, de exemplu, numarul de argumente, SF
sa fie DOAR 0 sau 1 sau daca comanda este valida. Daca am trecut de toate aceste
cazuri, am trimis mesajul de tip subscribe la server.
	Daca am primit un mesaj de tip unsubscribe am facut aceleasi valori ca si la
subscribe si am trimis mesajul catre server pentru a-l instiinta ca ma dezabonez
de la un topic.

	-daca nu am primit de la stdin, inseamna ca am primit un mesaj pe socket-ul
tcp de la server. 
	Daca am primit o comanda de tip exit, am dat break.
	Daca am primit o comanda de tipul "infotopic" inseamna ca am primit
actualizari pe topic-urile la care m-am abonat si afisez continutul mesajului.
	Daca am primit un mesaj de tipul "Id already in use.", afisez mesajul
"Connection refused.", pentru ca subscriberul trebuie sa stie ca i s-a refuzat
conexiunea.
	Altfel, orice ar fi, afisez mesajul.
