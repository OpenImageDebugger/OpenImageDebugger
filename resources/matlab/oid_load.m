%%
% Loads a binary file saved from Open Image Debugger

function buffer = oid_load(fname)

    fid = fopen(fname, 'r');

    type = fgets(fid);
    dimensions = fread(fid, 3, 'int32')';

    buffer = fread(fid, prod(dimensions), type(1:length(type)-1));

    rows = dimensions(1);
    cols = dimensions(2);
    channels = dimensions(3);

    buffer_t = reshape(reshape(buffer, channels, rows*cols)', [cols,rows,channels]);

    buffer = zeros(dimensions);
    for c = 1:channels
        buffer(:, :, c) = buffer_t(:, :, c)';
    end

    fclose(fid);

