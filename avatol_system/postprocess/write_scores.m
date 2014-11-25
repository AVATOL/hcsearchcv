function write_scores( outputPath, trainingList, scoringList, nonScoringList )
%WRITE_SCORES Write scores to output file.
%
%   outputPath      :   path/.../sorted_output_data_<charID>_<charName>.txt
%   trainingList    :   cell of structs where a struct denotes 
%                       a training instance and each struct has
%                           .pathToMedia: path to image
%                           .charState: character state
%                           .pathToAnnotation: path to annotation
%   scoringList     :   cell of structs where a struct denotes 
%                       a test/scoring instance and each struct has
%                           .pathToMedia: path to image
%                           .charState: character state inferred
%                           .pathToDetection: path to detection
%   nonScoringList  :   cell of structs where a struct denotes 
%                       a non-scored test instance and each struct has
%                           .pathToMedia: path to image

%% constants
DATA_DELIMITER = '|';

%% write to file
fid = fopen(outputPath, 'w');

for i = 1:length(trainingList)
    data = trainingList{i};
    fprintf(fid, 'training_data');
    fprintf(fid, '%s%s', DATA_DELIMITER, data.pathToMedia);
    fprintf(fid, '%s%s', DATA_DELIMITER, data.charState);
    fprintf(fid, '%s%s', DATA_DELIMITER, data.charStateName);
    fprintf(fid, '%s%s', DATA_DELIMITER, data.pathToAnnotation);
    fprintf(fid, '%s%s', DATA_DELIMITER, data.taxonID);
    fprintf(fid, '%s%s', DATA_DELIMITER, data.lineNumber);
    fprintf(fid, '\n');
end

for i = 1:length(scoringList)
    data = scoringList{i};
    fprintf(fid, 'image_scored');
    fprintf(fid, '%s%s', DATA_DELIMITER, data.pathToMedia);
    fprintf(fid, '%s%s', DATA_DELIMITER, data.charState);
    fprintf(fid, '%s%s', DATA_DELIMITER, data.charStateName);
    fprintf(fid, '%s%s', DATA_DELIMITER, data.pathToDetection);
    fprintf(fid, '%s%s', DATA_DELIMITER, data.taxonID);
    fprintf(fid, '%s%s', DATA_DELIMITER, '1'); %TODO line number
    fprintf(fid, '%s%s', DATA_DELIMITER, data.scoreConfidence);
    fprintf(fid, '\n');
end

for i = 1:length(nonScoringList)
    data = nonScoringList{i};
    fprintf(fid, 'image_not_scored');
    fprintf(fid, '%s%s', DATA_DELIMITER, data.pathToMedia);
    fprintf(fid, '%s%s', DATA_DELIMITER, data.taxonID);
    fprintf('\n');
end

fclose(fid);

end

