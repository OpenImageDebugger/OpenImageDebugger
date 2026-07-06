%%
% Loads a binary file saved from Open Image Debugger

function buffer = oid_load(fname)

    known_types = {'uint8', 'uint16', 'int16', 'int32', 'float'};
    bytes_per_elem = [1, 2, 2, 4, 4];

    fid = fopen(fname, 'rb');
    if fid < 0
        error('oid_load: cannot open file: %s', fname);
    end
    cleanup = onCleanup(@() fclose(fid)); %#ok<NASGU>

    probe = fread(fid, 6, 'uint8=>char')';
    type = '';
    type_len = 0;
    for i = 1:numel(known_types)
        candidate = known_types{i};
        if strncmp(probe, candidate, numel(candidate))
            type = candidate;
            type_len = numel(candidate);
            break;
        end
    end
    if isempty(type)
        error('oid_load: unknown matrix type in %s', fname);
    end

    fseek(fid, type_len, 'bof');
    next_byte = fread(fid, 1, 'uint8');

    if next_byte == 10
        dimensions = fread(fid, 3, 'int32')';
        raw = fread(fid, inf, type);
    else
        % Legacy export format (OID <= 1.x regression): ASCII dimensions
        fseek(fid, type_len, 'bof');
        dims_str = read_digit_run(fid);

        fseek(fid, 0, 'eof');
        file_size = ftell(fid);
        data_start = type_len + numel(dims_str);

        bpp = bytes_per_elem(strcmp(known_types, type));
        pixel_count = (file_size - data_start) / bpp;
        dimensions = parse_dimensions(dims_str, pixel_count);

        fseek(fid, data_start, 'bof');
        raw = fread(fid, pixel_count, type);
    end

    buffer = reshape_buffer(raw, dimensions);
end

function dims_str = read_digit_run(fid)
    dims_str = '';
    while true
        c = fread(fid, 1, 'uint8=>char');
        if isempty(c) || c < '0' || c > '9'
            if ~isempty(dims_str)
                fseek(fid, -1, 'cof');
            end
            break;
        end
        dims_str = [dims_str, c]; %#ok<AGROW>
    end
end

function dimensions = parse_dimensions(dims_str, pixel_count)
    n = numel(dims_str);
    for i = 1:(n - 2)
        for j = (i + 1):(n - 1)
            rows = str2double(dims_str(1:i));
            cols = str2double(dims_str(i + 1:j));
            channels = str2double(dims_str(j + 1:end));
            if rows > 0 && cols > 0 && channels > 0 && ...
                    rows * cols * channels == pixel_count
                dimensions = [rows, cols, channels];
                return;
            end
        end
    end
    error('oid_load: could not parse dimensions from ''%s''', dims_str);
end

function buffer = reshape_buffer(raw, dimensions)
    rows = dimensions(1);
    cols = dimensions(2);
    channels = dimensions(3);

    buffer_t = reshape(reshape(raw, channels, rows * cols)', ...
        [cols, rows, channels]);

    buffer = zeros(dimensions, class(raw));
    for c = 1:channels
        buffer(:, :, c) = buffer_t(:, :, c)';
    end
end
