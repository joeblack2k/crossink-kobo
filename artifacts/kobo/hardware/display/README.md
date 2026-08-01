# N437 refreshbewijs

De eigen DRM-backend heeft GC16/full, AUTO/partial en DU/partial op de fysieke
Kobo Glo HD uitgevoerd zonder ioctl-fout, kernelwaarschuwing, crash of
watchdogincrement. Na iedere reeks startte de normale GUI opnieuw.

De gebruiker heeft het fysieke scherm na deze reeksen bekeken en expliciet
bevestigd geen ghosting te hebben waargenomen. Daarmee is de veilige native
1072×1448 displaybasis voor Beta 1 bewezen.

De 100-update DU-reeks verhoogde de warmste gemeten thermal zone van 46,789 °C
naar maximaal 51,910 °C; na de test zakte deze naar 47,927 °C. Dit is alleen
functioneel en thermisch bewijs. Het is nog geen PASS voor acceptatiecriterium
15: visuele ghostingcontrole, een 1000-update duurtest en veilige vergelijking
van alle bruikbare waveforms ontbreken nog. Er is niet overgeklokt.

Gebruik de benchmarktool als volgt:

    crossink-kobo-display-smoke identity fast 100 200

Stop de GUI-service vooraf en wacht tot het applicatieproces werkelijk weg is;
anders kan de tool geen DRM master worden. Start de service na afloop altijd
opnieuw.
